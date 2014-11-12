Creating a HTTP server compatible with Smart IR Remote
======================================================

To create an HTTP server with basic capabilities that can accept HTTP commands and forward them through InfraRed, you will have to implement 3 methods: *status*, *send* and *record*. This server can run on anything (Arduino, Raspberry Pi, a PC, etc ...), as long as it implements the methods below. It must run on port *9009*.

Getting the connected ir blasters (currently called "anymotes")
-----------------
####Request format:
http://[ip]:9009/api/status/
####Response format:
{"response":"success","anymotes":[{"address":"84:DD:20:E6:A5:03","name":"AnyMote Home"},{"address":"84:DD:20:E6:A5:43","name":"Remote41Q"}]}



Sending a code through. 
-----------------
Codes are formatted in as a simple decimals array, with the first one being the command frequency (usually 38000 +/- 10%), and then pairs of pulses with the on/off timings of the command. Assuming a frequency of 38000Hz, if you recorded a command using another arduino and you have the code in microseconds format (almost everybody does), you'll have to divide each number by 26 to get the pulses format we use.

####Request Format:
http://[ip]:9009/api/send?address=84:DD:20:E6:A5:43&code=38028,172,172,22,64,22,64,22,64,22,21,22,21,22,21,22,21,22,21,22,64,22,64,22,64,22,21,22,21,22,21,22,21,22,21,22,64,22,64,22,64,22,21,22,21,22,21,22,21,22,21,22,21,22,21,22,21,22,64,22,64,22,64,22,64,22,64,22,1820,
####Response OK:
{"message":"command sent","status":"success"}
####Response FAIL:
{"message":"there is no connected IR Blaster with that address","status":"error"}



Record a new command
-----------------
####Request Format:
http://[ip]:9009/api/record?address=84:DD:20:E6:A5:43
####Response OK:
{"status":"success","code":"39000,174,176,20,66,20,66,20,66,20,23,20,23,20,23,20,23,20,23,20,66,20,66,20,66,20,23,20,23,20,23,20,23,20,23,20,66,20,66,20,23,20,66,20,23,20,23,20,23,20,23,20,23,20,23,20,66,20,23,20,66,20,66,20,66,20,66,20,1285,"}
####Response FAIL ir blaster not connected:
{"message":"there is no connected IR Blaster with that address","status":"error"}
####Response FAIL recording failed / timed out after 60 seconds:
{"message":"recording failed","status":"error"}