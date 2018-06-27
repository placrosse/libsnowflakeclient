/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeAzureClient.hpp"
#include <was/blob.h>
#include <was/storage_account.h>
#include <was/core.h>

#define AZURE_SAS_TOKEN "AZURE_SAS_TOKEN"

Snowflake::Client::SnowflakeAzureClient::SnowflakeAzureClient(
  StageInfo *stageInfo, unsigned int parallel)
{
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

