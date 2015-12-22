# ops-sysd Test Cases

[TOC]

##  image manifest read ##
### Objective ###
Verify that sysd correctly processes the image.manifest file.
### Requirements ###
 - Virtual Mininet Test Setup

### Setup ###
#### Topology Diagram ####
```
  [s1]
```
### Description ###
1. Bring up sysd with image.manifest file
 - Verify that daemon info in db matches what is in image.manifest file

### Test Result Criteria ###
#### Test Pass Criteria ####
All verifications pass.
#### Test Fail Criteria ####
One or more verifications fail.

## sysd hw desc files read ##
### Objective ###
Verify that sysd correctly processes the hardware description files.
### Requirements ###
 - Virtual Mininet Test Setup

### Setup ###
#### Topology Diagram ####
```
  [s1]
```
### Description ###
1. Bring up sysd with hardware desc files
 - Verify number\_ports is correct
 - Verify max\_bond\_count is correct
 - Verify max\_lag\_member\_count is correct
 - Verify switch\_device\_port is correct
 - Verify connector is correct
 - Verify bridge\_normal is correct
 - Verify vrf\_default is correct

### Test Result Criteria ###
#### Test Pass Criteria ####
All verifications pass.
#### Test Fail Criteria ####
One or more verifications fail.

## /etc/os-release file read ##
### Objective ###
Verify that sysd correctly processes the /etc/os-release file.
### Requirements ###
Virtual Mininet Test Setup

### Setup ###
#### Topology Diagram ####
```
  [s1]
```
### Description ###
1. Copy the sample os-releases files to the VSI switch /tmp directory
2. Stop the OVSDB server as well as sysd on the switch
3. Copy the specific os-release file, e.g os-release.ops-1.0.0, to the /etc/os-release file.
4. Start the OVSDB server as well as sysd on the switch
5. Verify that the software\_info.os\_name as well as
   switch\_version column of the System table in OVSDB shows the
   corresponding information stored in the /etc/os-release file.

### Test Result Criteria ###
#### Test Pass Criteria ####
- Verify OS name in the OVSDB is same with the appropriate /etc/os-release NAME entry
- Verify switch version in the OVSDB is same with the appropriate /etc/os-release VERSION\_ID and BUILD\_ID.
#### Test Fail Criteria ####
One of the verification fails, e.g. OS name in the OVSDB is different from the /etc/os-release NAME value.
