/*
Copyright Bubi Technologies Co., Ltd. 2017 All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef CONSENSUS_MSG_
#define CONSENSUS_MSG_

#include <proto/cpp/consensus.pb.h>

namespace bubi {
	class ConsensusMsg {
		int64_t seq_;
		std::string type_;
		protocol::PbftEnv pbft_env_;
		std::vector<std::string> values_;
		std::string node_address_;
		std::string hash_;
	public:
		ConsensusMsg() {}
		ConsensusMsg(const protocol::PbftEnv &pbft_env);
		~ConsensusMsg();

		bool operator < (const ConsensusMsg &msg) const;
		bool operator == (const ConsensusMsg &value_frm) const;
		int64_t GetSeq() const;
		std::vector<std::string> GetValues() const;
		const char *GetNodeAddress() const;
		std::string GetType() const;
		protocol::PbftEnv  GetPbft() const;
	};
}

#endif