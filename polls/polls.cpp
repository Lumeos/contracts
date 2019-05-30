#include "polls.hpp"

void polls::rmrfpolls() {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");

    std::vector<uint64_t> keysForDeletion;

    eosio::print("rmrfpolls");
    int i = 0;
    for(auto& item : _polls) {
        eosio::print(item.key);
        if (i++ < 50) {
            keysForDeletion.push_back(item.key);   
        } else {
            break;
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        eosio::print(key);
        auto itr = _polls.find(key);
        if (itr != _polls.end()) {
            _polls.erase(itr);
        }
    }
}

void polls::rmrfvotes() {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");
 
    std::vector<uint64_t> keysForDeletion;

    int i = 0;
    for(auto& item : _votes2) {
        if (i++ < 50) {
            keysForDeletion.push_back(item.key);   
        } else {
            break;
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        auto itr = _votes2.find(key);
        if (itr != _votes2.end()) {
            _votes2.erase(itr);
        }
    }
}

void polls::rmrfcomments() {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");

    std::vector<uint64_t> keysForDeletion;

    int i = 0;
    for(auto& item : _comments) {
        if (i++ < 50) {
            keysForDeletion.push_back(item.key);   
        } else {
            break;
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        auto itr = _comments.find(key);
        if (itr != _comments.end()) {
            _comments.erase(itr);
        }
    }
}

void polls::addpoll(eosio::name s, uint64_t pollId, uint64_t communityId) {
    eosio_assert(has_auth(s), "missing required authority of a lumeos user account");

    for(auto& item : _polls) {
        if (item.pollId == pollId) {
            eosio::print("Poll with id already exists: ", pollId);
            return; // if poll id already exists, stop
        }
    }

    // update the table to include a new poll
    _polls.emplace(get_self(), [&](auto& p) {
        p.key = _polls.available_primary_key();
        p.pollId = pollId;
        p.communityId = communityId;
        p.pollStatus = 0;
        p.voteCounts = 0;
        p.commentsCount = 0;
        p.likes = 0;
        p.dislikes = 0;
    });
}

void polls::rmpoll(uint64_t pollId) {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");
        
    std::vector<uint64_t> keysForDeletion;
    // find items which are for the named poll
    for(auto& item : _polls) {
        if (item.pollId == pollId) {
            keysForDeletion.push_back(item.key);   
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        eosio::print("remove from _polls ", key);
        auto itr = _polls.find(key);
        if (itr != _polls.end()) {
            _polls.erase(itr);
        }
    }


    // add remove votes ... don't need it the actions are permanently stored on the block chain

    std::vector<uint64_t> keysForDeletionFromVotes;
    // find items which are for the named poll
    for(auto& item : _votes2) {
        if (item.pollId == pollId) {
            keysForDeletionFromVotes.push_back(item.key);   
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletionFromVotes) {
        eosio::print("remove from _votes2 ", key);
        auto itr = _votes2.find(key);
        if (itr != _votes2.end()) {
            _votes2.erase(itr);
        }
    }
}

// Users have to trigger this action through the lumetokenctr::stakevote action
void polls::vote(uint64_t poll_id, uint64_t option, uint64_t amount, name voter) {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");

    // is the poll open
    for(auto& item : _polls) {
        if (item.pollId == poll_id) {
            if (item.pollStatus != 1) {
                return;
            }
            break; // only need to check status once
        }
    }

    // has account name already voted?  
    for(auto& vote : _votes2) {
        if (vote.pollId == poll_id && vote.account == voter) {
            return;
        }
    }

    // Create the stake object
    staketbl staketblobj(_self, _self.value);
    uint64_t stake_id = staketblobj.available_primary_key();
    staketblobj.emplace( _self, [&]( auto& s ) {
        s.id = stake_id;
        s.user = voter;
        s.amount = amount;
        s.timestamp = now();
        s.completion_time = now() + STAKING_DURATION;
    });

    // find the poll and the option and increment the count
    for(auto& item : _polls) {
        if (item.pollId == poll_id) {
            _polls.modify(item, get_self(), [&](auto& p) {
                p.voteCounts = p.voteCounts + 1;
            });
            break;
        }
    }

    // record that voter has voted
    _votes2.emplace(get_self(), [&](auto& pv){
        pv.key = _votes2.available_primary_key();
        pv.pollId = poll_id;
        pv.option = option;
        pv.account = voter;
    });
}

void polls::uppolllikes(uint64_t pollId, uint32_t likesCount, uint32_t dislikesCount, name accountName) {
    eosio_assert(has_auth(_self) || has_auth(accountName), "missing required authority of a lumeos user account");
    for(auto& item : _polls) {
        if (item.pollId == pollId) {
            _polls.modify(item, get_self(), [&](auto& p) {
                p.likes = likesCount;
                p.dislikes = dislikesCount;
            });
            break;
        }
    }
}

void polls::addcomment(uint64_t pollId, uint64_t commentId, name accountName) {
    eosio_assert(has_auth(_self) || has_auth(accountName), "missing required authority of a lumeos user account");

    for(auto& item : _comments) {
        if (item.commentId == commentId) {
            eosio::print("Comment with id already exists: ", commentId);
            return; // if comment id already exists, stop
        }
    }

    _comments.emplace(get_self(), [&](auto& cmnt){
        cmnt.key = _votes2.available_primary_key();
        cmnt.pollId = pollId;
        cmnt.commentId = commentId;
        cmnt.account = accountName;
        cmnt.likes = 1;
        cmnt.dislikes = 0;
    });

    for(auto& item : _polls) {
        if (item.pollId == pollId) {
            _polls.modify(item, get_self(), [&](auto& p) {
                p.commentsCount = p.commentsCount + 1;
            });
            break;
        }
    }
}

void polls::upcmntlikes(uint64_t commentId, uint32_t likesCount, uint32_t dislikesCount, name accountName) {
    eosio_assert(has_auth(_self) || has_auth(accountName), "missing required authority of a lumeos user account");
    for(auto& item : _comments) {
        if (item.commentId == commentId) {
            _comments.modify(item, get_self(), [&](auto& c) {
                c.likes = likesCount;
                c.dislikes = dislikesCount;
            });
            break;
        }
    }
}

EOSIO_DISPATCH( polls, (rmrfpolls)(rmrfvotes)(rmrfcomments)(addpoll)(rmpoll)(vote)(uppolllikes)(addcomment)(upcmntlikes))
