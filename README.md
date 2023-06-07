# iotexec
Execute RPC commands received from the iothub service

# Overview

The iotexec service connects to the iothub service using the libiotclient
library.  It waits for received messages directed to the "exec" target.
i.e received messages which have service:exec in the message header.

The exec service treats the body of the received message as a CLI command,
and executes the command and streams the command output back to the iothub
service for delivery to the cloud.

It takes the received message-id and stores it in the correlation-id header of
the response message.  This allows the message originator to track command
responses against the original command request.

DISCLAIMER:  There is no security implemented in this service.  Anyone
who can originate a cloud-to-device message from the IoT Hub can execute
a remote command on the target device.

## Prerequisites

The iotexec service requires the following components:

- libiotclient : IOT Client library ( https://github.com/tjmonk/libiotclient )
- iothub: IOTHub Service ( https://github.com/tjmonk/iothub )

## Command Line Arguments

```
usage: iotexec [-v] [-h]
 [-h] : display this help
 [-v] : verbose output
 ```

## Build

```
./build.sh
```

## Example

Before running the example, make sure the iothub service is running and
connected to the IoT Hub using a valid device connection string.

For example:

```
iothub -c "HostName=my-iot-hub.azure-devices.net;DeviceId=device-001;SharedAccessKey=ROpU5sG+XRIFWJeJGWCm7xLv8VIYyGx6vKfyNjPduAs=" &
```

Run the iotexec service

```
iotexec -v &
```

The azure portal can be used to send cloud-to-device messages.
Send a cloud to device command via the IoT Hub, with payload 'date'
and the following headers:
```
messageId:1f92da2a-c4da-4ef9-8d2a-ce7722ab487c
service:exec
```

You should see some diagnostic output and the exec service will execute the
date command and generate a response message with
correlationId:1f92da2a-c4da-4ef9-8d2a-ce7722ab487c

```
Waiting for data...
size: 4096
rxBuf = 0xaaaadfa693b0
mq = 4
prio = 0
n = 66
buffer length = 59
headers:
messageId:1f92da2a-c4da-4ef9-8d2a-ce7722ab487c
service:exec
p=0xaaaadfa693eb
header length=59
*ppHeader = messageId:1f92da2a-c4da-4ef9-8d2a-ce7722ab487c
service:exec
body length = 7
ProcessMessage!!
headerLength = 59
header (59): messageId:1f92da2a-c4da-4ef9-8d2a-ce7722ab487c
service:exec
body (7): date
messageId = 1f92da2a-c4da-4ef9-8d2a-ce7722ab487c
Tue Jun  6 21:33:24 UTC 2023
```


