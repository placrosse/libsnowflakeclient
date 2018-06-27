/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeAzureClient.hpp"
#include <was/blob.h>
#include <was/storage_account.h>
#include <was/core.h>

Snowflake::Client::SnowflakeAzureClient::SnowflakeAzureClient(
  StageInfo *stageInfo, unsigned int parallel)
{
  utility::string_t connectionString;
  computeConnectionString(stageInfo, connectionString);

  azure::storage::cloud_storage_account storage_account =
    azure::storage::cloud_storage_account::parse(connectionString);

}

void Snowflake::Client::SnowflakeAzureClient::computeConnectionString(
  StageInfo *stageInfo, std::string &connectionString)
{
  
}

