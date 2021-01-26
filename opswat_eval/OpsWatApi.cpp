#include "OpsWatApi.h"

OpsWatApi::OpsWatApi()
{
	hSessionApi = NULL;
	hConnectApi = NULL;
	hRequestApi = NULL;
	dataId = "";
	jsonString = "";
}

OpsWatApi::~OpsWatApi()
{
	/* close handles if they arent already close */
	CloseHandles();
}

void OpsWatApi::CloseHandles()
{
	/* if the handle isnt null close and null it out */
	if (hSessionApi != NULL)
	{
		WinHttpCloseHandle(hSessionApi);
		hSessionApi = NULL;
	}	
	if (hConnectApi != NULL)
	{
		WinHttpCloseHandle(hConnectApi);
		hConnectApi = NULL;
	}
	if (hRequestApi != NULL)
	{
		WinHttpCloseHandle(hRequestApi);
		hRequestApi = NULL;
	}
}

OpsWatApi::return_codes OpsWatApi::ConnectToApi()
{
	return_codes cc = success;
	hSessionApi = WinHttpOpen(PROGRAM_NAME, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

	if (hSessionApi)
	{
		hConnectApi = WinHttpConnect(hSessionApi, API_URI, 443, 0);
		if (!hConnectApi)
		{
			printf("ERROR: error getting session WinHttpConnect() %d \n", GetLastError());
			cc = undefined_error;
		}
	}
	else
	{
		printf("ERROR: error getting session WinHttpOpen() %d \n", GetLastError());
		cc = undefined_error;
	}
	return cc;
}

int OpsWatApi::GetStatusCode()
{
	int cc = 0;
	DWORD size = sizeof(DWORD);
	DWORD code = 0;
	if (hRequestApi != NULL)
	{
		WinHttpQueryHeaders(hRequestApi, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &size, NULL); /* used to get http status codes */
	}
	cc = code;
	return cc;
}

OpsWatApi::return_codes OpsWatApi::GetData(std::string* pJsonString)
{
	return_codes cc = success;

	char* pRecvData = NULL;
	DWORD size = 0;
	DWORD downloaded = 0;
	BOOL results = FALSE;
	do
	{
		size = 0;
		results = WinHttpQueryDataAvailable(hRequestApi, &size);
		if (!results)
			break;
		if (!size)
			break;

		pRecvData = (char *)calloc(size + 1, 1);
		if (pRecvData == NULL) /* check the amount of memory we have */
		{
			printf("ERROR: allocating %d bytes \n", size);
			cc = undefined_error;
			break;
		}

		results = WinHttpReadData(hRequestApi, (LPVOID)pRecvData, size, &downloaded);

		if (!results || !downloaded)
		{
			printf("ERROR: error reading data WinHttpReadData %d \n", GetLastError());
			cc = undefined_error;
			break;
		}

		pJsonString->append(pRecvData);

		free(pRecvData);

	} while (true); /* read until no more data */

	return cc;
}

OpsWatApi::return_codes OpsWatApi::GetDataId(Document* pDoc)
{
	dataId = "";
	return_codes cc = success;

	if (pDoc->HasMember("data_id"))
		dataId = (*pDoc)["data_id"].GetString();
	else
		cc = undefined_error;

	return cc;
}

OpsWatApi::return_codes OpsWatApi::Postfile(std::string* pFilePath, std::string* pApiKey)
{
	return_codes cc = success;
	USES_CONVERSION;
	char buffer[1000] = { 0 };
	BOOL results = FALSE;
	int ccStatusCode = 0;
	std::string fileName = "";
	Document doc;
	jsonString = "";

	cc = ConnectToApi();

	/* simple check to see if its a directory not just file name */
	if (pFilePath->rfind("\\") > 0)
		fileName = pFilePath->substr(pFilePath->rfind("\\") + 1);
	else
		fileName = *pFilePath;

	if (cc == success)
	{
		/* initalize request address */
		hRequestApi = WinHttpOpenRequest(hConnectApi, L"POST", POST_UPLOAD, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	}

	if (hRequestApi)
	{
		sprintf_s(buffer, API_KEY, pApiKey->c_str());
		/* add api key header */
		results = WinHttpAddRequestHeaders(hRequestApi, A2W(buffer), wcslen(A2W(buffer)), WINHTTP_ADDREQ_FLAG_ADD);
		if (!results)
		{
			printf("ERROR: error adding headers WinHttpAddRequestHeaders api key() %d \n", GetLastError());
			cc = undefined_error;
		}

		if (cc == success)
		{
			sprintf_s(buffer, FILE_NAME, fileName.c_str());
			results = WinHttpAddRequestHeaders(hRequestApi, A2W(buffer), wcslen(A2W(buffer)), WINHTTP_ADDREQ_FLAG_ADD);
			if (!results)
			{
				printf("ERROR: error adding headers WinHttpAddRequestHeaders file name () %d \n", GetLastError());
				cc = undefined_error;
			}
		}

		if (cc == success)
		{
			results = WinHttpAddRequestHeaders(hRequestApi, CONTENT_UPLOAD_HEADER, wcslen(CONTENT_UPLOAD_HEADER), WINHTTP_ADDREQ_FLAG_ADD);
			if (!results)
			{
				printf("ERROR: error adding headers WinHttpAddRequestHeaders content type  () %d \n", GetLastError());
				cc = undefined_error;
			}
		}
		
		if (cc == success)
		{
			results = WinHttpAddRequestHeaders(hRequestApi, TRANSFER_ENCODING, wcslen(TRANSFER_ENCODING), WINHTTP_ADDREQ_FLAG_ADD);
			if (!results)
			{
				printf("ERROR: error adding headers WinHttpAddRequestHeaders transfer encoding  () %d \n", GetLastError());
				cc = undefined_error;
			}
		}

	}
	else
	{
		printf("ERROR: error getting session WinHttpOpenRequest() %d \n", GetLastError());
		cc = undefined_error;
	}


	/* lets send the request */
	if (cc == success)
	{
		results = WinHttpSendRequest(hRequestApi, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 0, 0, 0, 0);
		if (!results)
		{
			printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
			cc = undefined_error;
		}
	}

	printf("IFNO: uploading file %s for analysis... \n", fileName.c_str());
	/* lets transfer the data */
	if (cc == success)
	{
		fstream fileHandle;
		unsigned char buffer[BLOCK] = { 0 };
		char lengthBuffer[10] = { 0 };
		uint64_t fileLength = 0;
		uint64_t pos = 0;
		uint64_t readSize = 0;
		DWORD bytesWritten = 0;

		fileHandle.open(pFilePath->c_str(), fstream::in | fstream::binary); /* open the file in binary mode */

		if (fileHandle.is_open())
		{
			/* get file length */
			fileHandle.seekg(0, fileHandle.end);
			fileLength = fileHandle.tellg();

			/* read in the data */
			while (fileLength != pos)
			{
				if (fileLength - pos >= BLOCK) /* either read in a block or whats left */
					readSize = BLOCK;
				else
					readSize = fileLength - pos;

				fileHandle.seekg(pos, fileHandle.beg);
				fileHandle.read((char *)buffer, readSize);

				sprintf_s(lengthBuffer, "%llX\r\n", readSize);

				/* send the header */
				if (!WinHttpWriteData(hRequestApi, lengthBuffer, strlen(lengthBuffer), &bytesWritten))
				{
					printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
					cc = undefined_error;
					break;
				}

				/* send the data */
				if (!WinHttpWriteData(hRequestApi, buffer, readSize, &bytesWritten))
				{
					printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
					cc = undefined_error;
					break;
				}

				/* send the footer  */
				if (!WinHttpWriteData(hRequestApi, "\r\n", 2, &bytesWritten))
				{
					printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
					cc = undefined_error;
					break;
				}

				pos += readSize;
				ZeroMemory(buffer, BLOCK);
			}

			/* send the termination  */
			if (!WinHttpWriteData(hRequestApi, "0\r\n\r\n", 5, &bytesWritten))
			{
				printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
				cc = undefined_error;
			}

			fileHandle.close();
		}
	}

	/* lets start recieveing the response  */
	if (cc == success)
	{
		results = WinHttpReceiveResponse(hRequestApi, NULL);
		if (!results)
		{
			printf("ERROR: sending recieving Response() %d \n", GetLastError());
			cc = undefined_error;
		}
	}
	else
	{
		printf("ERROR: sending recieving Response() %d  http response code : %d\n", GetLastError(), ccStatusCode);
		cc = undefined_error;
	}

	ccStatusCode = GetStatusCode();

	/* lets start receving the data */
	if (cc == success)
		cc = GetData(&jsonString);

	if (jsonString.length() > 0 && cc == success)
	{
		doc.Parse(jsonString.c_str());
		if (doc.HasParseError())
		{
			printf("ERROR: error parsing json %s\n", jsonString.c_str());
			cc = undefined_error;
		}
	}

	if (cc == success) /* check our status codes and for any errors */
	{
		int errorApi = 0;
		std::string message = "";
		if (ccStatusCode != 200 && ccStatusCode != 100) /* if it doesnt equal continue or success error out */
			printf("ERROR: error http status code returned %d \n", ccStatusCode);

		if (jsonString.length() > 0)/* lets try and extract the error code */
		{
			if (doc.HasMember("error")) /* some simple validation of the json could do more */
			{
				errorApi = doc["error"].operator[]("code").GetInt();
				message = doc["error"].operator[]("messages").GetArray()[0].GetString();
				printf("ERROR: error from api code: %d message: %s\n", errorApi, message.c_str());
			}
		}

		if (errorApi != 0)
			cc = undefined_error;
		else if (ccStatusCode != 200 && ccStatusCode != 100) /* catch anything we dont get a body from */
			cc = undefined_error;
	}

	/* lets get the data id for polling */
	if (cc == success)
	{
		cc = GetDataId(&doc);
		if (cc != success)
		{
			printf("ERROR: error parsing to get data id %s \n", jsonString.c_str());
			cc = undefined_error;
		}
	}
		

	CloseHandles();
	return cc;
}

OpsWatApi::return_codes OpsWatApi::GetFileAnalysis(Document* pDoc, std::string* pApiKey)
{
	return_codes cc = success;
	USES_CONVERSION;
	char buffer[1000] = { 0 };
	BOOL results = FALSE;
	int ccStatusCode = 0;
	jsonString = "";

	cc = ConnectToApi();

	if (cc == success)
	{
		sprintf_s(buffer, GET_UPLOAD, dataId.c_str());
		/* initalize request address */
		hRequestApi = WinHttpOpenRequest(hConnectApi, L"GET", A2W(buffer), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	}

	if (hRequestApi)
	{
		sprintf_s(buffer, API_KEY, pApiKey->c_str());
		/* add api key header */
		results = WinHttpAddRequestHeaders(hRequestApi, A2W(buffer), wcslen(A2W(buffer)), WINHTTP_ADDREQ_FLAG_ADD);
		if (!results)
		{
			printf("ERROR: error adding headers WinHttpAddRequestHeaders api key () %d \n", GetLastError());
			cc = undefined_error;
		}
	}
	else
	{
		printf("ERROR: error getting session WinHttpOpenRequest() %d \n", GetLastError());
		cc = undefined_error;
	}

	/* lets send the request */
	if (cc == success)
	{
		results = WinHttpSendRequest(hRequestApi, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 0, 0, 0, 0);
		if (!results)
		{
			printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
			cc = undefined_error;
		}
	}

	/* lets start recieveing the response  */
	if (cc == success)
	{
		results = WinHttpReceiveResponse(hRequestApi, NULL);
		if (!results)
		{
			printf("ERROR: sending recieving Response() %d \n", GetLastError());
			cc = undefined_error;
		}
	}
	else
	{
		printf("ERROR: sending recieving Response() %d  http response code : %d\n", GetLastError(), ccStatusCode);
		cc = undefined_error;
	}

	ccStatusCode = GetStatusCode();

	/* lets start receving the data */
	if (cc == success)
		cc = GetData(&jsonString);

	if (jsonString.length() > 0 && cc == success)
	{
		pDoc->Parse(jsonString.c_str());
		if (pDoc->HasParseError())
		{
			printf("ERROR: error parsing json %s\n", jsonString.c_str());
			cc = undefined_error;
		}
	}

	if (cc == success) /* check our status codes and for any errors */
	{
		int errorApi = 0;
		std::string message = "";
		if (ccStatusCode != 200 && ccStatusCode != 100) /* if it doesnt equal continue or success error out */
			printf("ERROR: error http status code returned %d \n", ccStatusCode);

		if (jsonString.length() > 0)/* lets try and extract the error code */
		{
			if (pDoc->HasMember("error")) /* some simple validation of the json could do more */
			{
				errorApi = (*pDoc)["error"].operator[]("code").GetInt();
				message = (*pDoc)["error"].operator[]("messages").GetArray()[0].GetString();
				printf("ERROR: error from api code: %d message: %s\n", errorApi, message.c_str());
			}
		}

		if (errorApi != 0)
			cc = undefined_error;
		else if (ccStatusCode != 200 && ccStatusCode != 100) /* catch anything we dont get a body from */
			cc = undefined_error;
	}

	CloseHandles();
	return cc;
}

OpsWatApi::return_codes OpsWatApi::GetFileAndPrint(std::string* pFilePath, std::string* pApiKey)
{
	return_codes cc = success;
	Document doc;

	printf("INFO: results are ready... \n");
	cc = GetFileAnalysis(&doc, pApiKey);

	if (cc == success)
		cc = PrintJsonResults(&doc);

	return cc;
}

OpsWatApi::return_codes OpsWatApi::WaitOnProcessing(std::string* pApiKey)
{
	return_codes cc = success;
	Document doc;

	do
	{
		cc = GetFileAnalysis(&doc, pApiKey);
		if (cc == success)
		{
			if (doc.HasMember("status")) /* file has not started processing post the status */
			{
				cc = file_processing;
				printf("INFO: Waiting on file to start processing status: %s last_updated: %s \n", doc["status"].GetString(), doc["last_updated"].GetString());
			}
			else if (doc.HasMember("scan_results"))
			{
				int percentFinished = doc["scan_results"].operator[]("progress_percentage").GetInt();// assuming its whole number percent based on api site
				if (percentFinished != 100)
				{
					cc = file_processing;
					printf("INFO: file still scanning progress %d percent \n", percentFinished);
				}
				else
					cc = success; /* success to break out we are done */
			}
			else
			{
				printf("ERROR: json objects not found when getting file analysis data \n");
				cc = undefined_error;
			}
		}

		if (cc == file_processing) /* sleep for 2 seconds while waiting */
			Sleep(2000);

	} while (cc == file_processing); /* while the file is being processed we will wait or error out if there is an issue */


	return cc;
}

OpsWatApi::return_codes OpsWatApi::UploadFileAndPrint(std::string* pFilePath, std::string* pApiKey)
{
	return_codes cc = success;

	cc = Postfile(pFilePath, pApiKey); /* upload the file */

	if (cc == success)
		cc = WaitOnProcessing(pApiKey); /* wait on processing */
	
	if (cc == success)
		cc = GetFileAndPrint(pFilePath, pApiKey); /* print the results */
	return cc;
}

OpsWatApi::return_codes OpsWatApi::CheckFileHashAndPrint(std::string* pHashStr, std::string* pApiKey)
{
	USES_CONVERSION;
	return_codes cc = success;
	char buffer[1000] = { 0 };
	BOOL results = FALSE;
	int ccStatusCode = 0;
	Document doc;
	jsonString = "";

	cc = ConnectToApi();

	if (cc == success)
	{
		sprintf_s(buffer, GET_HASH, pHashStr->c_str());
		/* initalize request address */
		hRequestApi = WinHttpOpenRequest(hConnectApi, L"GET", A2W(buffer), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	}

	if (hRequestApi)
	{
		sprintf_s(buffer, API_KEY, pApiKey->c_str());
		/* add api key header */
		results = WinHttpAddRequestHeaders(hRequestApi, A2W(buffer), wcslen(A2W(buffer)), WINHTTP_ADDREQ_FLAG_ADD);
		if (!results)
		{
			printf("ERROR: error adding headers WinHttpAddRequestHeaders api key () %d \n", GetLastError());
			cc = undefined_error;
		}
	}
	else
	{
		printf("ERROR: error getting session WinHttpOpenRequest() %d \n", GetLastError());
		cc = undefined_error;
	}



	/* lets send the request */
	if (cc == success)
	{
		results = WinHttpSendRequest(hRequestApi, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 0, 0, 0, 0);
		if (!results)
		{
			printf("ERROR: sending request WinHttpSendRequest() %d \n", GetLastError());
			cc = undefined_error;
		}
	}

	/* lets start recieveing the response  */
	if (cc == success )
	{
		results = WinHttpReceiveResponse(hRequestApi, NULL);
		if (!results)
		{
			printf("ERROR: sending recieving Response() %d \n", GetLastError());
			cc = undefined_error;
		}
	}
	else
	{
		printf("ERROR: sending recieving Response() %d  http response code : %d\n", GetLastError(), ccStatusCode);
		cc = undefined_error;
	}

	ccStatusCode = GetStatusCode();

	/* lets start receving the data */
	if (cc == success)
		cc = GetData(&jsonString);

	if (jsonString.length() > 0 && cc == success)
	{
		doc.Parse(jsonString.c_str());
		if (doc.HasParseError())
		{
			printf("ERROR: error parsing json %s\n", jsonString.c_str());
			cc = undefined_error;
		}
	}

	if (cc == success) /* check our status codes and for any errors */
	{
		int errorApi = 0;
		std::string message = "";
		if (ccStatusCode != 200 && ccStatusCode != 100) /* if it doesnt equal continue or success error out */
			printf("ERROR: error http status code returned %d \n", ccStatusCode);

		if (jsonString.length() > 0)/* lets try and extract the error code */
		{
			if (doc.HasMember("error")) /* some simple validation of the json could do more */
			{
				errorApi = doc["error"].operator[]("code").GetInt();
				message = doc["error"].operator[]("messages").GetArray()[0].GetString();
				printf("ERROR: error from api code: %d message: %s\n", errorApi, message.c_str());
			}
		}

		if (errorApi != 0)
		{
			if (errorApi == HASH_NOT_FOUND) /* set return code to hash not found so a scan can commence */
				cc = hash_not_found;
			else
				cc = undefined_error;
		}
		else if (ccStatusCode != 200 && ccStatusCode != 100) /* catch anything we dont get a body from */
			cc = undefined_error;
	}

	if (cc == success) /* ok we have data and http was success lets print out the data */
	{
		cc = PrintJsonResults(&doc);
		if (cc != success)
		{
			printf("ERROR: error parsing %s \n", jsonString.c_str());
			cc = undefined_error;
		}
	}
		

	CloseHandles();

	return cc;
}



OpsWatApi::return_codes OpsWatApi::PrintJsonResults(Document* pDoc)
{
	return_codes cc = success;

	if (pDoc->HasMember("scan_results") && pDoc->HasMember("file_info"))
	{
		if((*pDoc)["file_info"].HasMember("display_name"))
			printf("filename: %s\n", (*pDoc)["file_info"].operator[]("display_name").GetString());
		if((*pDoc)["scan_results"].HasMember("scan_all_result_i"))
			printf("overall_status: %s\n\n", (*pDoc)["scan_results"].operator[]("scan_all_result_i").GetInt() ? "Infected" : "Clean"); /* this is done because desired output is clean or infected,
        																													              api doesnt return that in scan_all_result_a it returns No Threat Detected */
	}
	else
		cc = undefined_error;

	if (pDoc->HasMember("scan_results") && (*pDoc)["scan_results"].HasMember("scan_details")) /* again simple validation can be better */
	{
		std::string engine = "";
		std::string threat_found = "";
		std::string def_time = "";
		int scan_result = 0;
		const Value& scanDetails = (*pDoc)["scan_results"].operator[]("scan_details");
		for (Value::ConstMemberIterator itr = scanDetails.MemberBegin(); itr != scanDetails.MemberEnd(); itr++)
		{
			/* extract the data and get the results */
			engine = (*itr).name.GetString();
			threat_found = (*itr).value.operator[]("threat_found").GetString();
			def_time = (*itr).value.operator[]("def_time").GetString();
			scan_result = (*itr).value.operator[]("scan_result_i").GetInt();

			/* print results */
			printf("engine: %s \n", engine.c_str());
			printf("threat_found: %s \n", threat_found.length() ? threat_found.c_str() : "Clean"); /* 0 meaning clean more than 0 meaning we have a threat */
			printf("scan_result: %d \n", scan_result);
			printf("def_time: %s \n\n", def_time.c_str());
		}

		printf("END");
	}
	else
		cc = undefined_error;

	return cc;
}