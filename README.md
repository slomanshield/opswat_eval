# opswat_eval

To build please pull down and extract the zip file onto a windows 10 machines running visual sutdio 2017 (enterprise peferred)

open the solution file with visual studio 2017

project settings have already selected release so just right click and build project. 

The output might come out quickly, if you missed it check the Release you should see some obj files, exe (with different time stamps for built files) and dlls

library used for hashing: openssl

library used for http: WinHttp (built into MFC)

library used for json: rapidjson (headers included for build)

to run please open a command prompt in the release directory and to use the .exe the commands are as follows

opswat_eval.exe upload_file <file_path> <api_key>

exmaple 

opswat_eval.exe upload_file "C:\Users\Danny\Desktop\test_file.txt" XXXXXXXXXXXXXXXXXXXXXX

where XXXXXXXXXXXXXXXXXXXXXX is the api key 
