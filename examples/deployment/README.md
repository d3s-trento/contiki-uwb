# DEPLOYMENT LIBRARY AND TEST

This code is used to set the node logical id given its MAC address.

The user should create a folder containing node's addresses for the given
testbed and the corresponding platform or modify the provided `mytestbed-evb1000`.

The folder must have the following file:

* `node-map.c` containing node's addresses and the corresponding
   logical id

* in order to run the `deployment-test` project the `project-conf.h`
  file should be created too.

Insert the ID and the corresponding MAC address of nodes
deployed to the `id_addr_list` array.

```c
 static struct id_addr id_addr_list[] = {

   {1, {0x00, 0x12, 0x4b, 0x00, 0x06, 0x0d ,0xb5, 0xf0}},
   {2, {0x00, 0x12, 0x4b, 0x00, 0x06, 0x0d ,0xb4, 0x59}},
   {3, {0x00, 0x12, 0x4b, 0x00, 0x06, 0x0d ,0xb4, 0x79}},
   {4, {0x00, 0x12, 0x4b, 0x00, 0x06, 0x0d ,0x9a, 0xd0}},
   {5, {0x00, 0x12, 0x4b, 0x00, 0x06, 0x0d ,0x9a, 0xca}},
   {5, {0x00, 0x12, 0x4b, 0x00, 0x06, 0x0d ,0xb1, 0x47}},

   {42, {0x10, 0x20, 0x5f, 0x8b, 0x10, 0x00, 0x38, 0x3c}},

 };
```

To discover the MAC addresses of your devices, compile and flash the
`deployment-test` application:

```
$ make TARGET=evb1000 deployment-test.upload
```

When the flashing is done, read the device serial output specifying the correct
serial port, e.g.:

```
$ make TARGET=evb1000 login PORT=/dev/ttyACM0
```

It will print the MAC address and the current node ID (if assigned, otherwise 0).
If the node ID is not assigned, you can do it in the `node-map.c` file.

## Using `deployment.h`

This example is meant to be used as dependency, outside the standalone
`deployment-test` project.

To exploit the deployment functionalities, it is required to
add this example test folder to your makefile and include the `deployment.c`
file.

In addition, the testbed folder has to be included.
For convenience a `TESTBED` variable can be created in the Makefile.target
file. The corresponding `node-map.c` is then made visible to the compilation
chain adding the following lines to the Makefile.

```Makefile
# path to this project folder
PROJECTDIRS += ../deployment ../deployment/$(TESTBED)
PROJECT_SOURCEFILES += deployment.c
```

Within the code, issue the following calls to load the
MAC address and set the corresponding node-id.
Variable `node_id` is then made directly
available once importing the `sys/node-id.h` Contiki header.

```c
deployment_load_ieee_addr();
deployment_set_node_id_ieee_addr();
```
