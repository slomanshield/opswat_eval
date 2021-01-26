/* std incldues */
#include <iostream>
#include <string>
#include <unordered_map>
#include <list>
#include <fstream>
#include <stdio.h>
#include <string.h>

using namespace std;

/* MFC includes including library for http */
#include <afxwin.h>         
#include <afxext.h>         
#include <afxdisp.h>        
#include <winhttp.h>

/* json library includes */
#include "..\rapidjson\document.h"
#include "..\rapidjson\writer.h"
#include "..\rapidjson\stringbuffer.h"
#include "..\rapidjson\encodings.h"

using namespace rapidjson;

/* openssl includes for hashing */
#include "openssl\err.h"
#include "openssl\sha.h"

#pragma comment(lib,"libeay32.lib")
#pragma comment(lib,"ssleay32.lib")

/* include winhttp library for linking */
#pragma comment(lib,"winhttp.lib")

#define FOR_NULL 1
#define BLOCK 4096


/* defines for HTTP */
#define PROGRAM_NAME L"OpsWatEval"
#define API_URI L"api.metadefender.com"
#define GET_HASH "/v4/hash/%s"
#define POST_UPLOAD L"/v4/file"
#define GET_UPLOAD "/v4/file/%s"
#define API_KEY "apikey: %s"
#define FILE_NAME "filename: %s"
#define CONTENT_UPLOAD_HEADER L"Content-Type: application/octet-stream"
#define TRANSFER_ENCODING L"Transfer-encoding: chunked"

#define HASH_NOT_FOUND 404003

typedef typename std::list<string>::const_iterator StringItr;

class OpsWatApi
{
	
	public:
		enum return_codes { success, hash_not_found, file_processing, undefined_error }; /* define error codes */

		OpsWatApi();
		~OpsWatApi();

		return_codes CheckFileHashAndPrint(std::string* pHashStr, std::string* pApiKey);
		return_codes UploadFileAndPrint(std::string* pFilePath, std::string* pApiKey);
	private:
		void CloseHandles();
		HINTERNET  hSessionApi, hConnectApi, hRequestApi; /* handles to do http requests */
		
		return_codes ConnectToApi();
		return_codes GetData(std::string* pJsonString);
		return_codes PrintJsonResults(Document* pDoc);
		return_codes Postfile(std::string* pFilePath, std::string* pApiKey);
		return_codes GetFileAndPrint(std::string* pFilePath, std::string* pApiKey);
		return_codes WaitOnProcessing(std::string* pApiKey);
		int GetStatusCode();
		return_codes GetDataId(Document* pDoc);
		return_codes GetFileAnalysis(Document* pDoc, std::string* pApiKey);
		std::string dataId;
		std::string jsonString;
};