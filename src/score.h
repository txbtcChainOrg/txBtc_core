#pragma once

#include "platform/memory.h"
#include "platform/m256.h"
#include "platform/concurrency.h"
#include "smart_contracts/math_lib.h"
#include "public_settings.h"

#include "score_cache.h"

////////// Scoring algorithm \\\\\\\\\\

template<
    unsigned int dataLength,
    unsigned int infoLength,
    unsigned int numberOfInputNeurons,
    unsigned int numberOfOutputNeurons,
    unsigned int maxInputDuration,
    unsigned int maxOutputDuration,
    unsigned int solutionBufferCount
>
struct ScoreFunction
{
    int miningData[dataLength];
    m256i initialRandomSeed;

#pragma warning(push)
#pragma warning(disable:4293)
    //need 2 set of variables for input and output
    static constexpr unsigned int SYNAPSE_CHUNK_SIZE_INPUT = (dataLength + numberOfInputNeurons + infoLength);
    static constexpr unsigned int SYNAPSE_CHUNK_SIZE_INPUT_BIT = (SYNAPSE_CHUNK_SIZE_INPUT + 7) >> 3;
    static constexpr unsigned int PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT = (((SYNAPSE_CHUNK_SIZE_INPUT_BIT + 7) >> 3) << 3);
    static constexpr unsigned int NEURON_SCANNED_ROUND_INPUT = PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT >> 3;
    static constexpr unsigned int LAST_ELEMENT_BIT_INPUT = (dataLength + numberOfInputNeurons + infoLength) & 63;
    static constexpr unsigned long long LAST_ELEMENT_MASK_INPUT = LAST_ELEMENT_BIT_INPUT == 0 ?
        0xFFFFFFFFFFFFFFFFULL : (0xFFFFFFFFFFFFFFFFULL >> (64 - LAST_ELEMENT_BIT_INPUT));

    static constexpr unsigned int SYNAPSE_CHUNK_SIZE_OUTPUT = (dataLength + numberOfOutputNeurons + infoLength);
    static constexpr unsigned int SYNAPSE_CHUNK_SIZE_OUTPUT_BIT = (SYNAPSE_CHUNK_SIZE_OUTPUT + 7) >> 3;
    static constexpr unsigned int PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT = (((SYNAPSE_CHUNK_SIZE_OUTPUT_BIT + 7) >> 3) << 3);
    static constexpr unsigned int NEURON_SCANNED_ROUND_OUTPUT = PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT >> 3;
    static constexpr unsigned int LAST_ELEMENT_BIT_OUTPUT = (dataLength + numberOfOutputNeurons + infoLength) & 63;
    static constexpr unsigned long long LAST_ELEMENT_MASK_OUTPUT = LAST_ELEMENT_BIT_OUTPUT == 0 ?
        0xFFFFFFFFFFFFFFFFULL : (0xFFFFFFFFFFFFFFFFULL >> (64 - LAST_ELEMENT_BIT_OUTPUT));
#pragma warning(pop)

    struct
    {
        int input[dataLength + numberOfInputNeurons + infoLength];
        int output[infoLength + numberOfOutputNeurons + dataLength];
    } neurons[solutionBufferCount];
    struct
    {
        char input[(numberOfInputNeurons + infoLength) * (dataLength + numberOfInputNeurons + infoLength)];
        char output[(numberOfOutputNeurons + dataLength) * (infoLength + numberOfOutputNeurons + dataLength)];
        unsigned short lengths[maxInputDuration * (numberOfInputNeurons + infoLength) + maxOutputDuration * (numberOfOutputNeurons + dataLength)];
    } synapses[solutionBufferCount];

    struct
    {
        char input_positive[(numberOfInputNeurons + infoLength) * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT];
        char input_negative[(numberOfInputNeurons + infoLength) * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT];
        char output_positive[(numberOfOutputNeurons + dataLength) * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT];
        char output_negative[(numberOfOutputNeurons + dataLength) * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT];
    } synapses1Bit[solutionBufferCount];

    volatile char solutionEngineLock[solutionBufferCount];

#if USE_SCORE_CACHE
    ScoreCache<SCORE_CACHE_SIZE, SCORE_CACHE_COLLISION_RETRIES> scoreCache;
#endif

    void initMiningData(m256i randomSeed)
    {
        initialRandomSeed = randomSeed; // persist the initial random seed to be able to sned it back on system info response
        random((unsigned char*)&randomSeed, (unsigned char*)&randomSeed, (unsigned char*)miningData, sizeof(miningData));
    }

    // Save score cache to SCORE_CACHE_FILE_NAME
    void saveScoreCache()
    {
#if USE_SCORE_CACHE
        scoreCache.save(SCORE_CACHE_FILE_NAME);
#endif
    }

    // Update score cache filename with epoch and try to load file
    bool loadScoreCache(int epoch)
    {
        bool success = true;
#if USE_SCORE_CACHE
        SCORE_CACHE_FILE_NAME[sizeof(SCORE_CACHE_FILE_NAME) / sizeof(SCORE_CACHE_FILE_NAME[0]) - 4] = epoch / 100 + L'0';
        SCORE_CACHE_FILE_NAME[sizeof(SCORE_CACHE_FILE_NAME) / sizeof(SCORE_CACHE_FILE_NAME[0]) - 3] = (epoch % 100) / 10 + L'0';
        SCORE_CACHE_FILE_NAME[sizeof(SCORE_CACHE_FILE_NAME) / sizeof(SCORE_CACHE_FILE_NAME[0]) - 2] = epoch % 10 + L'0';
        success = scoreCache.load(SCORE_CACHE_FILE_NAME);
#endif
        return success;
    }

    static void setBitNeuron(unsigned char* A, int idx) {
        int m = idx & 7;
        idx = idx >> 3;
        A[idx] |= ((unsigned char)1 << m);
    }
    static void clearBitNeuron(unsigned char* A, int idx) {
        int m = idx & 7;
        idx = idx >> 3;
        A[idx] &= ~((unsigned char)1 << m);
    }
    static void neuronU64To1Bit(unsigned long long u64, unsigned char* positive, unsigned char* negative) {
        for (int m = 0; m < 8; m++)
        {
            char val = char(((u64 >> (m * 8)) & 0xff) % 3) - 1;
            *positive &= ~((unsigned char)1 << m); // clear bit
            *positive |= ((unsigned char)(val > 0) << m); // set bit
            *negative &= ~((unsigned char)1 << m); // clear bit
            *negative |= ((unsigned char)(val < 0) << m); // set bit
        }
    }

    // main score function
    unsigned int operator()(const unsigned long long processor_Number, const m256i& publicKey, const m256i& nonce)
    {
        int score = 0;
#if USE_SCORE_CACHE
        unsigned int scoreCacheIndex = scoreCache.getCacheIndex(publicKey, nonce);
        score = scoreCache.tryFetching(publicKey, nonce, scoreCacheIndex);
        if (score >= scoreCache.MIN_VALID_SCORE)
        {
            return score;
        }
        score = 0;
#endif

        const unsigned long long solutionBufIdx = processor_Number % solutionBufferCount;
        ACQUIRE(solutionEngineLock[solutionBufIdx]);

        unsigned char nrVal1Bit[math_lib::max(PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT, PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT)];
        random(publicKey.m256i_u8, nonce.m256i_u8, (unsigned char*)&synapses[solutionBufIdx], sizeof(synapses[0]));
        for (unsigned int inputNeuronIndex = 0; inputNeuronIndex < numberOfInputNeurons + infoLength; inputNeuronIndex++)
        {
            unsigned long long* p = (unsigned long long*)(synapses[solutionBufIdx].input + inputNeuronIndex * (dataLength + numberOfInputNeurons + infoLength));
            for (unsigned int anotherInputNeuronIndex = 0; anotherInputNeuronIndex < (dataLength + numberOfInputNeurons + infoLength + 7) / 8; anotherInputNeuronIndex++)
            {
                const unsigned int offset = inputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT + anotherInputNeuronIndex;
                neuronU64To1Bit(p[anotherInputNeuronIndex], (unsigned char*)synapses1Bit[solutionBufIdx].input_positive + offset,
                    (unsigned char*)synapses1Bit[solutionBufIdx].input_negative + offset);
            }
        }

        for (unsigned int outputNeuronIndex = 0; outputNeuronIndex < numberOfOutputNeurons + dataLength; outputNeuronIndex++)
        {
            unsigned long long* p = (unsigned long long*)(synapses[solutionBufIdx].output + outputNeuronIndex * (dataLength + numberOfOutputNeurons + infoLength));
            for (unsigned int anotherOutputNeuronIndex = 0; anotherOutputNeuronIndex < (infoLength + numberOfOutputNeurons + dataLength + 7) / 8; anotherOutputNeuronIndex++)
            {
                const unsigned int offset = outputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT + anotherOutputNeuronIndex;
                neuronU64To1Bit(p[anotherOutputNeuronIndex], (unsigned char*)synapses1Bit[solutionBufIdx].output_positive + offset,
                    (unsigned char*)synapses1Bit[solutionBufIdx].output_negative + offset);
            }
        }

        for (unsigned int inputNeuronIndex = 0; inputNeuronIndex < numberOfInputNeurons + infoLength; inputNeuronIndex++)
        {
            unsigned char* ptr_positive = (unsigned char*)synapses1Bit[solutionBufIdx].input_positive + inputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT;
            unsigned char* ptr_negative = (unsigned char*)synapses1Bit[solutionBufIdx].input_negative + inputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT;
            clearBitNeuron(ptr_positive, (dataLength + inputNeuronIndex));
            clearBitNeuron(ptr_negative, (dataLength + inputNeuronIndex));
        }
        for (unsigned int outputNeuronIndex = 0; outputNeuronIndex < numberOfOutputNeurons + dataLength; outputNeuronIndex++)
        {
            unsigned char* ptr_positive = (unsigned char*)synapses1Bit[solutionBufIdx].output_positive + outputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT;
            unsigned char* ptr_negative = (unsigned char*)synapses1Bit[solutionBufIdx].output_negative + outputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT;
            clearBitNeuron(ptr_positive, (infoLength + outputNeuronIndex));
            clearBitNeuron(ptr_negative, (infoLength + outputNeuronIndex));
        }

        unsigned int lengthIndex = 0;

        copyMem(&neurons[solutionBufIdx].input[0], miningData, dataLength * sizeof(miningData[0]));
        setMem(&neurons[solutionBufIdx].input[sizeof(miningData) / sizeof(neurons[0].input[0])], sizeof(neurons[0]) - sizeof(miningData), 0);
        setMem(nrVal1Bit, sizeof(nrVal1Bit), 0);

        for (int i = 0; i < dataLength + numberOfInputNeurons + infoLength; i++) {
            if (neurons[solutionBufIdx].input[i] < 0) {
                setBitNeuron(nrVal1Bit, i);
            }
            else {
                clearBitNeuron(nrVal1Bit, i);
            }
        }

        for (unsigned int tick = 0; tick < maxInputDuration; tick++)
        {
            unsigned short neuronIndices[numberOfInputNeurons + infoLength];
            unsigned short numberOfRemainingNeurons = 0;
            for (numberOfRemainingNeurons = 0; numberOfRemainingNeurons < numberOfInputNeurons + infoLength; numberOfRemainingNeurons++)
            {
                neuronIndices[numberOfRemainingNeurons] = numberOfRemainingNeurons;
            }
            while (numberOfRemainingNeurons)
            {
                const unsigned short neuronIndexIndex = synapses[solutionBufIdx].lengths[lengthIndex++] % numberOfRemainingNeurons;
                const unsigned short inputNeuronIndex = neuronIndices[neuronIndexIndex];
                neuronIndices[neuronIndexIndex] = neuronIndices[--numberOfRemainingNeurons];
                unsigned long long* sy_pos = (unsigned long long*)(synapses1Bit[solutionBufIdx].input_positive + (inputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT));
                unsigned long long* sy_neg = (unsigned long long*)(synapses1Bit[solutionBufIdx].input_negative + (inputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_INPUT_BIT));
                int lv = 0;
                for (int i = 0; i < (NEURON_SCANNED_ROUND_INPUT - 1); i++) {
                    unsigned long long A0 = sy_pos[i];
                    unsigned long long A1 = sy_neg[i];
                    unsigned long long B = ((unsigned long long*)nrVal1Bit)[i];
                    int s = (int)__popcnt64(A0 ^ B);
                    s -= (int)__popcnt64(A1 ^ B);
                    lv += s;
                }
                {
                    unsigned long long A0 = (*(unsigned long long*)(sy_pos + (NEURON_SCANNED_ROUND_INPUT - 1)));
                    unsigned long long A1 = (*(unsigned long long*)(sy_neg + (NEURON_SCANNED_ROUND_INPUT - 1)));
                    unsigned long long B = (*(unsigned long long*)(nrVal1Bit + (NEURON_SCANNED_ROUND_INPUT - 1) * 8));
                    if (LAST_ELEMENT_BIT_INPUT != 0) {
                        A0 &= LAST_ELEMENT_MASK_INPUT;
                        A1 &= LAST_ELEMENT_MASK_INPUT;
                        B &= LAST_ELEMENT_MASK_INPUT;
                    }
                    int s = (int)__popcnt64(A0 ^ B);
                    s -= (int)__popcnt64(A1 ^ B);
                    lv += s;
                    neurons[solutionBufIdx].input[dataLength + inputNeuronIndex] += lv;
                    if (neurons[solutionBufIdx].input[dataLength + inputNeuronIndex] < 0) {
                        setBitNeuron(nrVal1Bit, dataLength + inputNeuronIndex);
                    }
                    else {
                        clearBitNeuron(nrVal1Bit, dataLength + inputNeuronIndex);
                    }
                }
            }
        }

        copyMem(&neurons[solutionBufIdx].output[0], &neurons[solutionBufIdx].input[dataLength + numberOfInputNeurons], infoLength * sizeof(neurons[0].input[0]));
        for (int i = 0; i < dataLength + numberOfOutputNeurons + infoLength; i++) {
            if (neurons[solutionBufIdx].output[i] < 0) {
                setBitNeuron(nrVal1Bit, i);
            }
            else {
                clearBitNeuron(nrVal1Bit, i);
            }
        }
        for (unsigned int tick = 0; tick < maxOutputDuration; tick++)
        {
            unsigned short neuronIndices[numberOfOutputNeurons + dataLength];
            unsigned short numberOfRemainingNeurons = 0;
            for (numberOfRemainingNeurons = 0; numberOfRemainingNeurons < numberOfOutputNeurons + dataLength; numberOfRemainingNeurons++)
            {
                neuronIndices[numberOfRemainingNeurons] = numberOfRemainingNeurons;
            }
            while (numberOfRemainingNeurons)
            {
                const unsigned short neuronIndexIndex = synapses[solutionBufIdx].lengths[lengthIndex++] % numberOfRemainingNeurons;
                const unsigned short outputNeuronIndex = neuronIndices[neuronIndexIndex];
                neuronIndices[neuronIndexIndex] = neuronIndices[--numberOfRemainingNeurons];
                unsigned long long* sy_pos = (unsigned long long*)(synapses1Bit[solutionBufIdx].output_positive + (outputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT));
                unsigned long long* sy_neg = (unsigned long long*)(synapses1Bit[solutionBufIdx].output_negative + (outputNeuronIndex * PADDED_SYNAPSE_CHUNK_SIZE_OUTPUT_BIT));
                int lv = 0;
                for (int i = 0; i < (NEURON_SCANNED_ROUND_OUTPUT - 1); i++) {
                    unsigned long long A0 = sy_pos[i];
                    unsigned long long A1 = sy_neg[i];
                    unsigned long long B = ((unsigned long long*)nrVal1Bit)[i];
                    int s = (int)__popcnt64(A0 ^ B);
                    s -= (int)__popcnt64(A1 ^ B);
                    lv += s;
                }
                {
                    unsigned long long A0 = (*(unsigned long long*)(sy_pos + (NEURON_SCANNED_ROUND_OUTPUT - 1)));
                    unsigned long long A1 = (*(unsigned long long*)(sy_neg + (NEURON_SCANNED_ROUND_OUTPUT - 1)));
                    unsigned long long B = (*(unsigned long long*)(nrVal1Bit + (NEURON_SCANNED_ROUND_OUTPUT - 1) * 8));
                    if (LAST_ELEMENT_BIT_OUTPUT != 0) {
                        A0 &= LAST_ELEMENT_MASK_OUTPUT;
                        A1 &= LAST_ELEMENT_MASK_OUTPUT;
                        B &= LAST_ELEMENT_MASK_OUTPUT;
                    }
                    int s = (int)__popcnt64(A0 ^ B);
                    s -= (int)__popcnt64(A1 ^ B);
                    lv += s;
                    neurons[solutionBufIdx].output[infoLength + outputNeuronIndex] += lv;
                    if (neurons[solutionBufIdx].output[infoLength + outputNeuronIndex] < 0) {
                        setBitNeuron(nrVal1Bit, infoLength + outputNeuronIndex);
                    }
                    else {
                        clearBitNeuron(nrVal1Bit, infoLength + outputNeuronIndex);
                    }
                }
            }
        }

        for (unsigned int i = 0; i < dataLength; i++)
        {
            if ((miningData[i] >= 0) == (neurons[solutionBufIdx].output[infoLength + numberOfOutputNeurons + i] >= 0))
            {
                score++;
            }
        }
        RELEASE(solutionEngineLock[solutionBufIdx]);
#if USE_SCORE_CACHE
        scoreCache.addEntry(publicKey, nonce, scoreCacheIndex, score);
#endif
        return score;
    }

    
    volatile char taskQueueLock = 0;
    struct {
        m256i publicKey[NUMBER_OF_TRANSACTIONS_PER_TICK];
        m256i nonce[NUMBER_OF_TRANSACTIONS_PER_TICK];
    } taskQueue;
    unsigned int _nTask;
    unsigned int _nProcessing;
    unsigned int _nFinished;
    bool _nIsTaskQueueReady;

    void resetTaskQueue()
    {
        ACQUIRE(taskQueueLock);
        _nTask = 0;
        _nProcessing = 0;
        _nFinished = 0;
        _nIsTaskQueueReady = false;
        RELEASE(taskQueueLock);
    }

    // add task to the queue
    // queue size is limited at NUMBER_OF_TRANSACTIONS_PER_TICK 
    void addTask(m256i publicKey, m256i nonce)
    {   
        ACQUIRE(taskQueueLock);
        if (_nTask < NUMBER_OF_TRANSACTIONS_PER_TICK)
        {
            unsigned int index = _nTask++;
            taskQueue.publicKey[index] = publicKey;
            taskQueue.nonce[index] = nonce;
        }
        RELEASE(taskQueueLock);
    }

    void startProcessTaskQueue()
    {
        ACQUIRE(taskQueueLock);
        _nIsTaskQueueReady = true;
        RELEASE(taskQueueLock);
    }

    void stopProcessTaskQueue()
    {
        ACQUIRE(taskQueueLock);
        _nIsTaskQueueReady = false;
        RELEASE(taskQueueLock);
    }

    // get a task, can call on any thread
    bool getTask(m256i* publicKey, m256i* nonce)
    {
        if (!_nIsTaskQueueReady)
        {
            return false;
        }
        bool result = false;
        ACQUIRE(taskQueueLock);
        if (_nProcessing < _nTask)
        {
            unsigned int index = _nProcessing++;
            *publicKey = taskQueue.publicKey[index];
            *nonce = taskQueue.nonce[index];
            result = true;
        }
        else
        {
            result = false;
        }
        RELEASE(taskQueueLock);
        return result;
    }
    void finishTask()
    {
        ACQUIRE(taskQueueLock);
        _nFinished++;
        RELEASE(taskQueueLock);
    }

    bool isTaskQueueProcessed()
    {
        return _nFinished == _nTask;
    }

    void tryProcessSolution(unsigned long long processorNumber)
    {
        m256i publicKey;
        m256i nonce;
        bool res = this->getTask(&publicKey, &nonce);
        if (res)
        {
            (*this)(processorNumber, publicKey, nonce);
            this->finishTask();
        }
    }
};
