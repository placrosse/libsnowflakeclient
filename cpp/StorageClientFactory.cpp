/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/SnowflakeTransferException.hpp"
#include "StorageClientFactory.hpp"
#include "SnowflakeAzureClient.hpp"
#include "SnowflakeS3Client.hpp"
#include "logger/SFLogger.hpp"

namespace Snowflake
{
namespace Client
{

IStorageClient * StorageClientFactory::injectedClient = nullptr;

IStorageClient * StorageClientFactory::getClient(
  StageInfo *stageInfo, unsigned int parallel,
  TransferConfig * transferConfig)
{
  switch (stageInfo->stageType)
  {
    case StageType::S3:
      CXX_LOG_INFO("Creating S3 client");
      return new SnowflakeS3Client(stageInfo, parallel, transferConfig);
    case StageType::MOCKED_STAGE_TYPE:
      return injectedClient;
    case StageType::AZURE:
      CXX_LOG_INFO("Creating Azure client");
      return new SnowflakeAzureClient(stageInfo, parallel);
    default:
      // invalid stage type
      throw SnowflakeTransferException(TransferError::UNSUPPORTED_FEATURE,
        "Remote storage not supported.");
  }
}

void StorageClientFactory::injectMockedClient(IStorageClient *client)
{
  injectedClient = client;
}

}
}
