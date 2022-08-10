# Tutorial

This tutorial explains how to configure the sputnik engine of the SmartMet Server when using Docker.

## Prereqs

Docker software has been installed on some Linux server where you have access to already and the smartmetserver docker container is up and running.

### File sputnik.conf

The purpose of the configuration file "sputnik.conf" is to define the frontend and backend server parameters. Frontend parameters are a list of IP addresses and UDP ports that the backend servers are listening to. Backend parameters are defined so that they are listening one of these addresses. For example when the frontend sends a service request to a broadcast address then the same broadcast address should be configured in the backend as "udpListenerAddress" and "udpListenerPort".

If you followed the “SmartMet Server Tutorial (Docker)” you have your configuration folders and files in the host machine at $HOME/docker-smartmetserver/smartmetconf but inside Docker they show up under /etc/smartmet. 

1. Go to the correct directory and enter command below to review the file:

```
$ less sputnik.conf
```
You will see something like this:
```
#####################  FRONTEND PARAMETERS #####################

# List of IP addresses and UDP ports that the backend servers are listening. The
# frontend sends its "ServiceDiscovery" requests to these UDP addresses. Notice
# that this list can contain direct IP addresses and broadcast addresses.

backendUdpListeners = ["192.168.122.255:31337","192.168.122.135:31338","192.168.122.135:31339"];

# The backend servers should be configured so that they are listening one of these
# addresses. I.e. when the frontend sends a service request to a brodcast address
# then the same broadcast address should be configured in the backend as "udpListenerAddress"
# and "udpListenerPort". The same is true when the frontend is using direct IP addresses.
# In this case the backend should listen exactly the same addresses that the frontend is using.

# For example:
#  TYPE           FRONTEND                BACKEND (udpListenerAddress+udpListenerPort)
#  Broadcast      192.168.100.255:31337   192.168.100.255:31337
#  Direct IP      192.168.122.135:31000   192.168.122.135:31000
#  Direct IP      192.168.122.135:32000   192.168.122.135:32000
#  Direct IP      192.168.122.138:31000   192.168.122.138:31000



#####################  BACKEND PARAMETERS ######################

hostname = "localhost";
httpAddress = "192.168.122.135";
httpPort = 8090;
udpListenerAddress = "192.168.122.255";
udpListenerPort = 31337;
comment = "Brainstorm server in myhost";
```
2. Use Nano or some other editor to modify **frontend** parameters with your own values:

```
backendUdpListeners = ["192.168.122.255:31337","192.168.122.135:31338","192.168.122.135:31339"];
```
3. Use Nano or some other editor to modify **backend** parameters with your own values:

```
hostname = "localhost";
httpAddress = "192.168.122.135";
httpPort = 8090;
udpListenerAddress = "192.168.122.255";
udpListenerPort = 31337;
comment = "SmartMet server in myhost";
```