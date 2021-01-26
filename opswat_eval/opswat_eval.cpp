#include "OpsWatApi.h"

bool do_uploadfile = false;

/* command list for valid commands */
std::unordered_map<std::string, bool*> commandList (
{
	{ "upload_file", &do_uploadfile } 
});

/* create typedef for easier use */
typedef typename std::unordered_map<std::string, bool*>::const_iterator CommandMapItr;

int HashFile(std::string* pFilePath, std::string* pOutputHash);

/* Notes: Things that could be improved, 
          Create a file IO object intead of using fstream and while loops when reading the file 
		  Create a HTTP object intead of copying the same WinHttp commands over and over 
		  And instead of passing pointers everywhere I could either pass in the variables needed on constructor or
		  make an Init type function */

int main(int argc, char** argv)
{
	int cc = 0;
	std::string command = "";
	std::string filePath = "";
	std::string apiKey = "";
	std::string hashStr = "";
	CommandMapItr itr;
	OpsWatApi api;

	/* some simple arg checking can be done better */
	if (argc != 4)
	{
		printf("ERROR: valid commands are \n"
			   "upload_file <file_path> <api_key>");
	}

	command = argv[1]; /* get command*/
	filePath = argv[2]; /* get file path*/;
	apiKey = argv[3]; /* get api key */

	itr = commandList.find(command);

	if(itr != commandList.end()) /* set command to true */
		*(*itr).second = true;

	if (do_uploadfile == true)
	{
		cc = HashFile(&filePath, &hashStr); /* hash the file */

		if (cc == 0)
		{
			cc = api.CheckFileHashAndPrint(&hashStr, &apiKey); /* lets see if it already exists in OpsWat database */
			if (cc == OpsWatApi::hash_not_found)
			{
				printf("INFO: hash not found will now upload... \n");
				cc = api.UploadFileAndPrint(&filePath, &apiKey); /* ok lets upload and get results */
			}
		}
		else
			printf("ERROR: hashing %s with error %d \n", filePath.c_str(), cc);
	}
	else
	{
		printf("ERROR: unknown command found %s \n", command.c_str());
		cc = -1;
	}

	return cc;
}

int HashFile(std::string* pFilePath, std::string* pOutputHash)
{
	int cc = 0;
	/*init sha context */
	SHA256_CTX hashContext;
	SHA256_Init(&hashContext);
	unsigned char buffer[BLOCK] = { 0 };
	unsigned char hash[SHA256_DIGEST_LENGTH] = { 0 };
	char hashHex[2 + FOR_NULL] = { 0 };

	fstream fileHandle;
	fileHandle.open(pFilePath->c_str(), fstream::in | fstream::binary); /* open the file in binary mode */

	if (fileHandle.is_open())
	{
		uint64_t fileLength = 0;
		uint64_t pos = 0;
		uint64_t readSize = 0;
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

			SHA256_Update(&hashContext, buffer, (size_t)readSize);

			pos += readSize;
			ZeroMemory(buffer, BLOCK);
		}
		/* get the binary hash */
		SHA256_Final(hash, &hashContext);
		fileHandle.close();

		/* get ascii representation */
		for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
		{
			sprintf_s(hashHex, "%02X", hash[i]);
			pOutputHash->append(hashHex);
		}
	}
	else
	{
		printf("ERROR: opening file %s \n", pFilePath->c_str());
		cc = -1;
	}

	return cc;
}