/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP

#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"

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

  ~SnowflakeAzureClient();

private:
  void computeConnectionString(StageInfo *stageInfo, std::string &connectionString);
};
}
}
#endif //SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP
