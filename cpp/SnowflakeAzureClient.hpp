/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP

#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include <was/blob.h>

namespace Snowflake
{
namespace Client
{
/**
 * Wrapper over Azure blob storage client
 */
class SnowflakeAzureClient : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeAzureClient(StageInfo *stageInfo, unsigned int parallel);

  ~SnowflakeAzureClient() {}

  virtual RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                                             std::basic_iostream<char> *dataStream);

  virtual RemoteStorageRequestOutcome download(FileMetadata *fileMetadata,
                                             std::basic_iostream<char> *dataStream);

  virtual RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata);

private:
  void computeConnectionString(StageInfo *stageInfo, std::string &connectionString);

  void extractContainerAndKey(std::string* filePathFull, utility::string_t& container,
    std::string& blobName);

  azure::storage::cloud_blob_client m_blobClient;

  StageInfo *m_stageInfo;

  bool checkRemoteBlobDuplicated(FileMetadata * fileMetadata);

  void addUserMetadata(azure::storage::cloud_metadata &metadata, FileMetadata * fileMetadata);
};
}
}
#endif //SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP
