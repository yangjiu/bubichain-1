﻿/*
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

#include <common/storage.h>
#include <common/pb2json.h>
#include <ledger/ledger_manager.h>
#include "account.h"
#include "kv_trie.h"

namespace bubi {

	//AccountFrm::AccountFrm() {
	//	utils::AtomicInc(&bubi::General::account_new_count);
	//	assets_ = nullptr;
	//	storage_ = nullptr;
	//}

	AccountFrm::AccountFrm(protocol::Account account_info) 
		: account_info_(account_info) {
		utils::AtomicInc(&bubi::General::account_new_count);
	}

	AccountFrm::AccountFrm(std::shared_ptr<AccountFrm> account){
		account_info_.CopyFrom(account->ProtocolAccount());
		assets_ = account->assets_;
		metadata_ = account->metadata_;
	}

	AccountFrm::~AccountFrm() {
		utils::AtomicInc(&bubi::General::account_delete_count);
	}

	std::string AccountFrm::Serializer() {
		return account_info_.SerializeAsString();
	}

	bool AccountFrm::UnSerializer(const std::string &str) {
		if (!account_info_.ParseFromString(str)) {
			LOG_ERROR("Accountunserializer is erro!");
			return false;
		}
		return true;
	}


	std::string AccountFrm::GetAccountAddress()const {
		return account_info_.address();
	}

	
	bool AccountFrm::UpdateSigner(const std::string &signer, int64_t weight) {
		weight = weight & UINT8_MAX;
		if (weight > 0) {
			bool found = false;
			for (int32_t i = 0; i < account_info_.mutable_priv()->signers_size(); i++) {
				if (account_info_.mutable_priv()->signers(i).address() == signer) {
					found = true;
					account_info_.mutable_priv()->mutable_signers(i)->set_weight(weight);
				}
			}

			if (!found) {
				if (account_info_.priv().signers_size() >= protocol::Signer_Limit_SIGNER) {
					return false;
				}

				protocol::Signer* signer1 = account_info_.mutable_priv()->add_signers();
				signer1->set_address(signer);
				signer1->set_weight(weight);
			}
		}
		else {
			bool found = false;
			std::vector<std::pair<std::string, int64_t> > nold;
			for (int32_t i = 0; i < account_info_.mutable_priv()->signers_size(); i++) {
				if (account_info_.mutable_priv()->signers(i).address() != signer) {
					nold.push_back(std::make_pair(account_info_.mutable_priv()->signers(i).address(), account_info_.mutable_priv()->signers(i).weight()));
				}
				else {
					found = true;
				}
			}

			if (found) {
				account_info_.mutable_priv()->clear_signers();
				for (size_t i = 0; i < nold.size(); i++) {
					protocol::Signer* signer = account_info_.mutable_priv()->add_signers();
					signer->set_address(nold[i].first);
					signer->set_weight(nold[i].second);
				}
			}
		}

		return true;
	}

	const int64_t AccountFrm::GetTypeThreshold(const protocol::Operation::Type type) const {
		const protocol::AccountThreshold &thresholds = account_info_.priv().thresholds();
		for (int32_t i = 0; i < thresholds.type_thresholds_size(); i++) {
			if (thresholds.type_thresholds(i).type() == type) {
				return thresholds.type_thresholds(i).threshold();
			}
		}

		return 0;
	}

	bool AccountFrm::UpdateTypeThreshold(const protocol::Operation::Type type, int64_t threshold) {
		threshold = threshold & UINT64_MAX;
		if (threshold > 0) {
			protocol::AccountThreshold *thresholds = account_info_.mutable_priv()->mutable_thresholds();
			bool found = false;
			for (int32_t i = 0; i < thresholds->type_thresholds_size(); i++) {
				if (thresholds->type_thresholds(i).type() == type) {
					found = true;
					thresholds->mutable_type_thresholds(i)->set_threshold(threshold);
				}
			}

			if (!found) {
				if (thresholds->type_thresholds_size() >= protocol::Signer_Limit_SIGNER) {
					return false;
				}

				protocol::OperationTypeThreshold* signer1 = thresholds->add_type_thresholds();
				signer1->set_type(type);
				signer1->set_threshold(threshold);
			}
		}
		else {
			bool found = false;
			protocol::AccountThreshold *thresholds = account_info_.mutable_priv()->mutable_thresholds();
			std::vector<std::pair<protocol::Operation::Type, int32_t> > nold;
			for (int32_t i = 0; i < thresholds->type_thresholds_size(); i++) {
				if (thresholds->type_thresholds(i).type() != type) {
					nold.push_back(std::make_pair(thresholds->type_thresholds(i).type(), thresholds->type_thresholds(i).threshold()));
				}
				else {
					found = true;
				}
			}

			if (found) {
				thresholds->clear_type_thresholds();
				for (size_t i = 0; i < nold.size(); i++) {
					protocol::OperationTypeThreshold* signer = thresholds->add_type_thresholds();
					signer->set_type(nold[i].first);
					signer->set_threshold(nold[i].second);
				}
			}
		}
		return true;
	}


	void AccountFrm::ToJson(Json::Value &result) {
		result = bubi::Proto2Json(account_info_);
	}

	void AccountFrm::GetAllAssets(std::vector<protocol::Asset>& assets){
		KVTrie trie;
		auto batch = std::make_shared<WRITE_BATCH>();
		std::string prefix = ComposePrefix(General::ASSET_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		trie.Init(Storage::Instance().account_db(), batch, prefix, 1);
		std::vector<std::string> values;
		trie.GetAll("", values);
		for (size_t i = 0; i < values.size(); i++){
			protocol::Asset asset;
			asset.ParseFromString(values[i]);
			assets.push_back(asset);
		}
	}

	void AccountFrm::GetAllMetaData(std::vector<protocol::KeyPair>& metadata){
		KVTrie trie;
		auto batch = std::make_shared<WRITE_BATCH>();
		std::string prefix = ComposePrefix(General::METADATA_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		trie.Init(Storage::Instance().account_db(), batch, prefix, 1);
		std::vector<std::string> values;
		trie.GetAll("", values);
		for (size_t i = 0; i < values.size(); i++){
			protocol::KeyPair asset;
			asset.ParseFromString(values[i]);
			metadata.push_back(asset);
		}
	}

	bool AccountFrm::GetAsset(const protocol::AssetProperty &asset_property, protocol::Asset& asset){
		//LOG_INFO("%p GetAsset", this);
		auto it = assets_.find(asset_property);
		if (it != assets_.end()){
			if (it->second.action_ == utils::DEL){
				return false;
			}
			asset.CopyFrom(it->second.data_);
			return true;
		}

		auto batch = std::make_shared<WRITE_BATCH>();
		std::string asset_prefix = ComposePrefix(General::ASSET_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		KVTrie trie;
		trie.Init(Storage::Instance().account_db(), batch, asset_prefix, 1);

		auto asset_key = asset_property.SerializeAsString();
		std::string buff;
	
		if (!trie.Get(asset_key, buff)){
			return false;
		}

		DataCache<protocol::Asset> Rec;
		Rec.action_ = utils::MOD;
		
		if (!asset.ParseFromString(buff)){
			BUBI_EXIT("fatal error,Asset ParseFromString fail, data may damaged");
		}
		Rec.data_.CopyFrom(asset);
		assets_.insert({ asset_property, Rec });
		return true;
	}

	void AccountFrm::SetAsset(const protocol::Asset& data_ptr){
		DataCache<protocol::Asset> Rec;
		Rec.action_ = utils::ADD;
		Rec.data_.CopyFrom(data_ptr);
		assets_[data_ptr.property()] = Rec;
	}

	//
	bool AccountFrm::GetMetaData(const std::string& binkey, protocol::KeyPair& keypair_ptr){
		//return assets_->GetEntry(asset_property, asset);
		auto it = metadata_.find(binkey);
		if (it != metadata_.end()){
			if (it->second.action_ == utils::DEL){
				return false;
			}
			keypair_ptr = it->second.data_;
			return true;
		}

		auto batch = std::make_shared<WRITE_BATCH>();
		KVTrie trie;
		std::string prefix =  ComposePrefix(General::METADATA_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		trie.Init(Storage::Instance().account_db(), batch, prefix, 1);

		std::string buff;
		if (!trie.Get(binkey, buff)){
			return false;
		}
		
		if (!keypair_ptr.ParseFromString(buff)){
			BUBI_EXIT("fatal error,Asset ParseFromString fail, data may damaged");
		}
		DataCache<protocol::KeyPair> Rec;
		Rec.action_ = utils::MOD;
		Rec.data_.CopyFrom(keypair_ptr);
		metadata_.insert({ binkey, Rec });

		return true;
	}

	void AccountFrm::SetMetaData(const protocol::KeyPair& dataptr){
		DataCache<protocol::KeyPair> Rec;
		Rec.action_ = utils::ADD;
		Rec.data_.CopyFrom(dataptr);
		metadata_[dataptr.key()] = Rec;
	}

	bool AccountFrm::DeleteMetaData(const std::string key){
		auto it = metadata_.find(key);
		if (it != metadata_.end()){
			if (it->second.action_ == utils::DEL){
				return false;
			}
			it->second.action_ = utils::DEL;
			return true;
		}

		std::string dbkey = account_info_.address();
		dbkey += "S";
		dbkey += key;
		auto db = Storage::Instance().account_db();
		std::string buff;
		if (!db->Get(key, buff)){
			return false;
		}
		DataCache<protocol::KeyPair> Rec;
		Rec.action_ = utils::DEL;
		metadata_.insert({ key, Rec });
		return true;
	}

	void AccountFrm::UpdateHash(std::shared_ptr<WRITE_BATCH> batch){
		KVTrie trie_asset;
		std::string asset_prefix = ComposePrefix(General::ASSET_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		trie_asset.Init(Storage::Instance().account_db(), batch, asset_prefix, 1);

		KVTrie trie_metadata;
		std::string meta_prefix = ComposePrefix(General::METADATA_PREFIX, utils::String::HexStringToBin(account_info_.address()));
		trie_metadata.Init(Storage::Instance().account_db(), batch, meta_prefix, 1);

		auto& map = assets_;
		for (auto it = map.begin(); it != map.end(); it++){
			auto action = it->second.action_;
			auto asset = it->second.data_;
			Json::Value tmp = Proto2Json(asset);
			switch (action)
			{
			case utils::ChangeAction::ADD:
			case utils::ChangeAction::MOD:
				if (asset.amount() == 0)
					trie_asset.Delete(asset.property().SerializeAsString());
				else
					trie_asset.Set(asset.property().SerializeAsString(), asset.SerializeAsString());
				break;
			case utils::ChangeAction::DEL:
				trie_asset.Delete(asset.property().SerializeAsString());
				break;

			default:
				break;
			}
		}
		trie_asset.UpdateHash();
		account_info_.set_assets_hash(trie_asset.GetRootHash());
		
		for (auto it = metadata_.begin(); it != metadata_.end(); it++){
			auto action = it->second.action_;
			auto kp = it->second.data_;

			switch (action)
			{
			case utils::ADD:
			case utils::MOD:
				trie_metadata.Set(it->first, kp.SerializeAsString());
				break;
			case utils::DEL:
				trie_metadata.Delete(it->first);
				break;

			default:
				break;
			}
		}
		trie_metadata.UpdateHash();
		account_info_.set_metadatas_hash(trie_metadata.GetRootHash());
	}

	void AccountFrm::NonceIncrease(){
		int64_t new_nonce = account_info_.nonce() + 1;
		account_info_.set_nonce(new_nonce);
	}
}

