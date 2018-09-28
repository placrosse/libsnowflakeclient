/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeAzureClient.hpp"
#include "util/Base64.hpp"
#include <was/blob.h>
#include <was/storage_account.h>
#include <was/core.h>
#include <cpprest/interopstream.h>

#define AZURE_SAS_TOKEN "AZURE_SAS_TOKEN"
#define AZURE_ENCDATA_KEY "encryptiondata"

namespace Snowflake
{
namespace Client
{

Snowflake::Client::SnowflakeAzureClient::SnowflakeAzureClient(
  StageInfo *stageInfo, unsigned int parallel):
  m_stageInfo(stageInfo)
{
  azure::storage::operation_context::set_default_log_level(
    azure::storage::client_log_level::log_level_verbose);

  utility::string_t connectionString;
  computeConnectionString(stageInfo, connectionString);

  azure::storage::cloud_storage_account storage_account =
    azure::storage::cloud_storage_account::parse(connectionString);

  m_blobClient = storage_account.create_cloud_blob_client();
}

void Snowflake::Client::SnowflakeAzureClient::computeConnectionString(
  StageInfo *stageInfo, std::string &connectionString)
{
  utility::string_t blobEndpoint = "https://" + stageInfo->storageAccount + "."
                                   + stageInfo->endPoint;

  utility::string_t sasToken(stageInfo->credentials.at(AZURE_SAS_TOKEN));
  connectionString = "BlobEndpoint=" + blobEndpoint +
    ";SharedAccessSignature=" + sasToken;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::upload(
  FileMetadata *fileMetadata, std::basic_iostream<char> *dataStream)
{
  utility::string_t containerName, blobName;
  utility::string_t filePathFull = m_stageInfo->location + fileMetadata->destFileName;
  extractContainerAndKey(&filePathFull, containerName, blobName);

  azure::storage::cloud_blob_container container = m_blobClient.
    get_container_reference(containerName);

  azure::storage::cloud_block_blob file = container.get_block_blob_reference(blobName);

  if (file.exists())
  {
    file.download_attributes();
    azure::storage::cloud_metadata & metadata = file.metadata();
    return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
  }

  azure::storage::cloud_metadata userMetadata;
  addUserMetadata(userMetadata, fileMetadata);
  file.metadata() = userMetadata;

  concurrency::streams::stdio_istream<uint8_t> b(*dataStream);
  file.upload_from_stream(b);
  file.upload_metadata();


}

void SnowflakeAzureClient::addUserMetadata(azure::storage::cloud_metadata &metadata,
  FileMetadata * fileMetadata)
{
  char ivEncoded[64];
  Snowflake::Client::Util::Base64::encode(
    fileMetadata->encryptionMetadata.iv.data,
    Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES),
    ivEncoded);

  size_t ivEncodeSize = Snowflake::Client::Util::Base64::encodedLength(
    Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES));

  std::string iv(ivEncoded, ivEncodeSize);

  std::stringstream ss;
  ss << "{\"EncryptionMode\":\"FullBlob\",\"WrappedContentKey\""
     << ":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\""
     << fileMetadata->encryptionMetadata.enKekEncoded
     << "\", \"Algorithm\":\"AES_CBC_256\"},\"EncryptionAgent\":"
     << "{\"Protocol\":\"1.0\",\"EncryptionAlgorithm\":"
     << "\"AES_CBC_256\"},\"ContentEncryptionIV\":\""
     << iv << "\",\"KeyWrappingMetadata\":{\"EncryptionLibrary\":"
     << "\"Openssl 1.1.0g\"}}";

  metadata.insert({SFC_DIGEST, fileMetadata->sha256Digest});
  metadata.insert({AZURE_ENCDATA_KEY, ss.str()});
}

bool SnowflakeAzureClient::checkRemoteBlobDuplicated(FileMetadata *fileMetadata)
{
  return true;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::download(
  FileMetadata *fileMetadata, std::basic_iostream<char> *dataStream)
{
}

RemoteStorageRequestOutcome SnowflakeAzureClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
}

void SnowflakeAzureClient::extractContainerAndKey(
  std::string *fileFullPath, utility::string_t& container, std::string& blobName)
{
  size_t sepIndex = fileFullPath->find_first_of('/');
  container = fileFullPath->substr(0, sepIndex);
  blobName = fileFullPath->substr(sepIndex + 1);
}

}
}
