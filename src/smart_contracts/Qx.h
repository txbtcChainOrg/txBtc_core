using namespace QPI;

struct QX2
{
};

struct QX
{
public:
	struct Fees_input
	{
	};
	struct Fees_output
	{
		uint32 assetIssuanceFee; // Amount of qus
		uint32 transferFee; // Amount of qus
		uint32 tradeFee; // Number of billionths
	};

	struct IssueAsset_input
	{
		uint64 name;
		sint64 numberOfShares;
		uint64 unitOfMeasurement;
		sint8 numberOfDecimalPlaces;
	};
	struct IssueAsset_output
	{
		long long issuedNumberOfShares;
	};

	struct TransferShareOwnershipAndPossession_input
	{
		id issuer;
		id possessor;
		id newOwner;
		unsigned long long assetName;
		long long numberOfShares;
	};
	struct TransferShareOwnershipAndPossession_output
	{
		long long transferredNumberOfShares;
	};

private:
	uint64 _earnedAmount;
	uint64 _distributedAmount;
	uint64 _burnedAmount;

	uint32 _assetIssuanceFee; // Amount of qus
	uint32 _transferFee; // Amount of qus
	uint32 _tradeFee; // Number of billionths

	PUBLIC(Fees)

		output.assetIssuanceFee = state._assetIssuanceFee;
		output.transferFee = state._transferFee;
		output.tradeFee = state._tradeFee;
	_

	PUBLIC(IssueAsset)

		if (invocationReward() < state._assetIssuanceFee)
		{
			if (invocationReward() > 0)
			{
				transfer(invocator(), invocationReward());
			}

			output.issuedNumberOfShares = 0;
		}
		else
		{
			if (invocationReward() > state._assetIssuanceFee)
			{
				transfer(invocator(), invocationReward() - state._assetIssuanceFee);
			}
			state._earnedAmount += state._assetIssuanceFee;

			output.issuedNumberOfShares = issueAsset(input.name, invocator(), input.numberOfDecimalPlaces, input.numberOfShares, input.unitOfMeasurement);
		}
	_

	PUBLIC(TransferShareOwnershipAndPossession)

		if (invocationReward() < state._transferFee)
		{
			if (invocationReward() > 0)
			{
				transfer(invocator(), invocationReward());
			}

			output.transferredNumberOfShares = 0;
		}
		else
		{
			if (invocationReward() > state._transferFee)
			{
				transfer(invocator(), invocationReward() - state._transferFee);
			}
			state._earnedAmount += state._transferFee;

			output.transferredNumberOfShares = transferShareOwnershipAndPossession(input.assetName, input.issuer, invocator(), input.possessor, input.numberOfShares, input.newOwner) < 0 ? 0 : input.numberOfShares;
		}
	_

	REGISTER_USER_FUNCTIONS

		REGISTER_USER_FUNCTION(Fees, 1);
	_

	REGISTER_USER_PROCEDURES

		REGISTER_USER_PROCEDURE(IssueAsset, 1);
		REGISTER_USER_PROCEDURE(TransferShareOwnershipAndPossession, 2);
	_

	INITIALIZE

		// No need to initialize _earnedAmount and other variables with 0, whole contract state is zeroed before initialization is invoked

		state._assetIssuanceFee = 1000000000;
		state._transferFee = 1000000;
		state._tradeFee = 5000000; // 0.5%
	_

	BEGIN_EPOCH
	_

	END_EPOCH
	_

	BEGIN_TICK
	_

	END_TICK
	_

	EXPAND
	_
};

