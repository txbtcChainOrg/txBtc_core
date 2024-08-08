
## txBtc_chain Wallet User Guide

### Contents
1. Introduction
2. Installation Guide
3. Using the Wallet Software
   - Node
   - Core Wallet
   - Block Explorer
   - PRC Service
4. Mining Software
5. FAQ

### 1. Introduction
The txBtc_chain wallet software integrates four main functional modules: Core Wallet, Node, Block Explorer, and PRC Service. Users need to start the Node module first before using other modules. This document will detail the usage of each module and related commands.

### 2. Installation Guide
To use the wallet software, please follow these steps to download and install it:

1. Download the executable file (`txBtc_core`) from GitHub.
2. Open a terminal and navigate to the directory where the executable file is located.
3. Make the file executable (if necessary):
   ```sh
   chmod +x txBtc_core
   ```

### 3. Using the Wallet Software

First, start the wallet executable file:
```sh
./txBtc_core
```

#### Node
Starting the node is a prerequisite for using other wallet functions. Here are the commands for the Node module:

1. **Enter the Node module**:
   ```sh
   node
   ```

2. **Sync block data**:
   ```sh
   sync [daemon | start]
   ```
   - `daemon`: Background sync, no sync info displayed.
   - `start`: Sync complete and automatically start node.

3. **Start node** (after synchronization):
   ```sh
   start
   ```
   Upon running the `start` command, you will be prompted to configure the node:
   - Enter the node port number.
   - Specify whether the node is an external node.

   Example of the prompt sequence:
   ```sh
   There is currently no node configuration file, which will be generated for you soon.
   Please enter the node port number (default: 29000):
   Is this node an external node? (Yes/No)
   ```

   After providing the necessary configuration, the node configuration file will be generated, and the node will start using the specified port.

#### Core Wallet
The Core Wallet is used to manage local wallets and addresses, and perform transactions. Here are the commands for the Core Wallet:

1. **Enter the Core Wallet module**:
   ```sh
   wallet
   ```

2. **Show local wallet list**:
   ```sh
   ls
   ```

3. **Create a new wallet**:
   ```sh
   create
   ```

4. **Import wallet (BIP39 mnemonic words)**:
   ```sh
   import
   ```

5. **Open a specific wallet**:
   ```sh
   open <wallet_name>
   ```

6. **Exit the wallet interface and return to the main interface**:
   ```sh
   backed
   ```

7. **Show available commands**:
   ```sh
   help
   ```

##### Wallet Management
After creating, importing, or opening a wallet, you can perform the following operations:

1. **Show address list**:
   ```sh
   ls
   ```

2. **Create a new address**:
   ```sh
   create
   ```

3. **Import wallet address (private key)**:
   ```sh
   import
   ```

4. **Open an address for operation**:
   ```sh
   open <index | title | address>
   ```

5. **Exit the address interface and return to the wallet interface**:
   ```sh
   backed
   ```

##### Address Management
After selecting and opening an address, you can perform the following operations:

1. **List the tokens held at the current address**:
   ```sh
   ls
   ```

2. **Send tokens**:
   ```sh
   send [token]
   ```
   If `[token]` is not specified, the mainnet coin will be sent by default.

3. **Receive tokens**:
   ```sh
   receive
   ```

4. **Export the private key of the current address**:
   ```sh
   export
   ```

#### Block Explorer
The Block Explorer is used to retrieve and view block data. Here are the commands for the Block Explorer:

1. **Enter the Block Explorer module**:
   ```sh
   explorer
   ```

2. **Get the latest 10 block data**:
   ```sh
   ls
   ```

3. **Search data**:
   ```sh
   search [address | tx | blockHash | blockHeight]
   ```
   - `address`: Search address details.
   - `tx`: Search transaction details.
   - `blockHash`: Search block details by hash.
   - `blockHeight`: Search block details by height.

4. **Get current block statistics**:
   ```sh
   status
   ```

#### PRC Service
The PRC Service module is currently under development. Please stay tuned for updates.

### 4. Mining Software

To use the mining software, run the following command:
```sh
txMining -s <server_ip> -p <port_number> -u <wallet_address> -w <worker_name> -t <temperature_limit>
```

#### Command-line Options
- **Server Address**:
  ```sh
  -s, --server <char>
  ```
  The server address to connect to.

- **Port**:
  ```sh
  -p, --port <char>
  ```
  The mining pool port.

- **Wallet Address**:
  ```sh
  -u, --user <char>
  ```
  The wallet address or mining pool login.

- **Worker Name**:
  ```sh
  -w, --worker <char>
  ```
  The name of the mining worker.

- **Worker Password (optional)**:
  ```sh
  --pass <char>
  ```
  The worker password or default pool password.

- **Temperature Limit**:
  ```sh
  -t, --templimit <char>
  ```
  When the GPU reaches this temperature limit, it stops mining until it cools down. The default value is 90°C.

**Important Note**: You must first run the local node and configure the port before starting mining. The `server_ip` should be the IP of the device where the node software is running, and the `port_number` should be the port configured in the node software.

### 5. FAQ

1. **How to sync block data?**
   Use the `sync [daemon | start]` command in the Node module to sync block data, choosing `daemon` for background sync or `start` to automatically start the node after sync completes.

2. **How to create and import a wallet?**
   Enter the Core Wallet module using the `wallet` command, then use `create` to create a new wallet and `import` to import BIP39 mnemonic words.

3. **How to send and receive tokens?**
   After opening an address in the Core Wallet module, use `send [token]` to send tokens and `receive` to display the receiving address.

4. **How to start the mining software?**
   Run the `txMining` command with the appropriate options for server IP, port, wallet address, worker name, and temperature limit. Ensure the node is running and properly configured before starting mining.

For additional questions or further assistance, please contact technical support.
