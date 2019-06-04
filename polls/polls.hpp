#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>

using namespace eosio;

using std::string;

const name TOKEN_CONTRACT = name("lumetokenctr");
const uint64_t STAKING_DURATION = 5 * 86400; // 5 days

CONTRACT polls : public eosio::contract {

public:
    using contract::contract;

    polls( name receiver, name code, datastream<const char*> ds ): contract(receiver, code, ds), _polls(receiver, code.value), _votes2(receiver, code.value), _comments(receiver, code.value)
    {}

    ACTION rmrfpolls(); //admin action
    ACTION rmrfvotes(); //admin action
    ACTION rmrfcomments(); //admin action

    ACTION addpoll(eosio::name s, uint64_t pollId, uint64_t communityId);
    ACTION rmpoll(uint64_t pollId); //admin action
    ACTION uppolllikes(uint64_t pollId, uint32_t likesCount, uint32_t dislikesCount, name accountName);

    ACTION addcomment(uint64_t pollId, uint64_t commentId, name accountName);
    ACTION upcmntlikes(uint64_t commentId, uint32_t likesCount, uint32_t dislikesCount, name accountName);

    ACTION vote(uint64_t poll_id, uint64_t option, uint64_t amount, name voter);

    //private: -- not private so the cleos get table call can see the table data.

    TABLE poll
    {
        uint64_t      key;                // primary key - pollId
        uint64_t      communityId = 0;    // which community the poll belongs to
        uint8_t       pollStatus = 0;     // staus where 0 = closed, 1 = open, 2 = finished
        uint32_t      voteCounts = 0;     // the number of votes for each itme -- this to be pulled out to separte table.
        uint32_t      commentsCount = 0;
        uint32_t      likes = 0;
        uint32_t      dislikes = 0;

        uint64_t primary_key() const { return key; }
    };
    typedef eosio::multi_index<"poll"_n, poll> pollstable;

    // V2 of pollvotes (pollvotes is deleted now)
    TABLE pollvotes2
    {
        uint64_t     key; 
        uint64_t     pollId;
        uint32_t     option;
        name         account; //this account has voted, use this to make sure no one votes > 1

        uint64_t primary_key() const { return key; }
        uint64_t by_pollId() const { return pollId; }
    };
    typedef eosio::multi_index<"pollvotes2"_n, pollvotes2, eosio::indexed_by<"pollid"_n, eosio::const_mem_fun<pollvotes2, uint64_t, &pollvotes2::by_pollId>>> votes2;

    TABLE comments
    {
        uint64_t     key; 
        uint64_t     pollId;
        uint32_t     likes = 0;
        uint32_t     dislikes = 0;
        name         account;

        uint64_t primary_key() const { return key; }
    };
    typedef eosio::multi_index<"comments"_n, comments> commentstable;

    //// local instances of the multi indexes
    pollstable _polls;
    votes2 _votes2;
    commentstable _comments;
};