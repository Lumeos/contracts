#include "lumetokenctr.hpp"
#include <eosio/transaction.hpp>
#include <chrono>

using namespace eosio;

void lumetokenctr::create( name issuer, asset maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}

void lumetokenctr::issue( name to, asset quantity, string memo )
{

    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
      SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
                          { st.issuer, to, quantity, memo }
      );
    }
}

void lumetokenctr::transfer( name from, name to, asset quantity, string memo )
{
    check( from != to, "Cannot transfer to self" );

    require_auth(from);
    
    check( is_account( to ), "to account does not exist");

    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch, make sure that the token name is LUME, and that you are specifying it with 3 decimal places of precision" );
    check( memo.size() <= 256, "Memo is too long. It must be 256 characters or less" );
    
    convert_locked_to_balance( from );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void lumetokenctr::burn( name owner, asset quantity )
{
    require_auth(owner);

    convert_locked_to_balance( owner );

    accounts from_acnts( _self, owner.value );
    stats statstable( _self, quantity.symbol.code().raw());

    sub_balance( owner, quantity );

    auto currency = statstable.find(quantity.symbol.code().raw());
    check(currency != statstable.end(), "Invalid token");
    statstable.modify( currency, same_payer, [&]( auto & entry ) {
        entry.supply -= quantity;
        entry.max_supply -= quantity;
    });
}

void lumetokenctr::lock( name owner, asset quantity, uint8_t days )
{
    require_auth(owner);

    accounts from_acnts( _self, owner.value );
    auto acnt_itr = from_acnts.find(quantity.symbol.code().raw());
    check(acnt_itr != from_acnts.end(), "Account with this asset does not exist");

    convert_locked_to_balance( owner );

    check(acnt_itr->balance - acnt_itr->locked >= quantity, "Not enough unlocked funds available to lock up, the maximum possible quantity that you can lock is " + (acnt_itr->balance - acnt_itr->locked).to_string());
    check(days <= 100, "You can not lock your tokens for more than 100 days");
    check(days > 0, "You can not lock your tokens for less than 1 day");

    from_acnts.modify(acnt_itr, owner, [&](auto & entry) {
        entry.locked += quantity;
    });

    locked_funds locked( _self, owner.value );
    auto itr = locked.find(days);
    if(itr == locked.end()) {
        locked.emplace(owner, [&](auto & entry) {
            entry.lock_time = days;
            entry.quantity = quantity;
        });
    } else {
        locked.modify(itr, owner, [&](auto & entry) {
            entry.quantity += quantity;
        });
    }
}

void lumetokenctr::unlock( name owner, asset quantity )
{
    accounts from_acnts( _self, owner.value );
    locked_funds locked( _self, owner.value );
    unlocking_funds unlocking( _self, owner.value );

    auto acnt_itr = from_acnts.find(quantity.symbol.code().raw());
    check(acnt_itr != from_acnts.end(), "Account with this asset does not exist");
    convert_locked_to_balance( owner );
    check(acnt_itr->locked >= quantity, "You can not unlock more than is currently locked. The maximum you can unlock is " + acnt_itr->locked.to_string());

    while(quantity.amount > 0) {
        auto locked_itr = locked.begin();
        if(locked_itr == locked.end()) break;
        auto wait = locked_itr->lock_time;
        asset unlock_quantity;
        if(locked_itr->quantity <= quantity) {
            unlock_quantity = locked_itr->quantity;
            quantity -= locked_itr->quantity;
            locked.erase(locked_itr);
        } else {
            unlock_quantity = quantity;
            locked.modify(locked_itr, same_payer, [&](auto & entry) {
                entry.quantity -= quantity;
            });
            quantity.amount = 0;
        }
        unlocking.emplace(owner, [&](auto & entry) {
            entry.id = unlocking.available_primary_key();
            entry.unlocked_at = time_point(microseconds(eosio::current_time_point().time_since_epoch()._count + 1000000 * wait*60*60*24));
            entry.quantity = unlock_quantity;
        });
    }
}

void lumetokenctr::transferlock ( name from, name to, asset quantity, string memo, uint64_t days)
{

    check( from != to, "Cannot transfer to self" );

    require_auth(_self);
    
    check( is_account( to ), "to account does not exist");

    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch, make sure that the token name is LUME, and that you are specifying it with 3 decimal places of precision" );
    check( memo.size() <= 256, "Memo is too long. It must be 256 characters or less" );
    
    convert_locked_to_balance( from );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );

    accounts from_acnts( _self, to.value );
    auto acnt_itr = from_acnts.find(quantity.symbol.code().raw());
    check(acnt_itr != from_acnts.end(), "Account with this asset does not exist");
    convert_locked_to_balance( to );

    check(acnt_itr->balance - acnt_itr->locked >= quantity, "Not enough unlocked funds available to lock up, the maximum possible quantity that you can lock is " + (acnt_itr->balance - acnt_itr->locked).to_string());
    check(days <= 730, "You can not lock your tokens for more than 2 years");
    check(days > 0, "You can not lock your tokens for less than 1 day");

    from_acnts.modify(acnt_itr, from, [&](auto & entry) {
        entry.locked += quantity;
    });

    locked_funds locked( _self, to.value );
    auto itr = locked.find(days);
    if(itr == locked.end()) {
        locked.emplace(from, [&](auto & entry) {
            entry.lock_time = days;
            entry.quantity = quantity;
        });
    } else {
        locked.modify(itr, from, [&](auto & entry) {
            entry.quantity += quantity;
        });
    }

}

void lumetokenctr::convert_locked_to_balance( name owner )
{
    accounts from_acnts( _self, owner.value );
    locked_funds locked( _self, owner.value );
    unlocking_funds unlocking( _self, owner.value );
    
    auto itr = unlocking.begin();
    while(itr != unlocking.end()) {
        if(itr->unlocked_at > eosio::current_time_point()) {
            ++itr;
            continue;
        }
        auto acnt_itr = from_acnts.find(itr->quantity.symbol.code().raw());
        check( acnt_itr->locked >= itr->quantity, "Trying to claim more tokens from unlocking than are available in your locked balance. Please contact a member of the Lumeos team on Telegram (@lumeos) or by email (team@lumeos.io)." );
        from_acnts.modify(acnt_itr, same_payer, [&](auto & entry) {
            entry.locked -= itr->quantity;
        });
        itr = unlocking.erase(itr);
    }
}

void lumetokenctr::sub_balance( name owner, asset value ) {
    accounts from_acnts( _self, owner.value );

    const auto& from = from_acnts.get( value.symbol.code().raw(), "No balance object found, you do not own any LUME" );
    check( from.balance.amount >= value.amount, "Overdrawn balance, only " + from.balance.to_string() + " is available" );
    check( from.balance - from.locked >= value, "You are attempting to transfer " + value.to_string() + ", but you can only transfer " + (from.balance - from.locked).to_string() + ", because " + from.locked.to_string() + " are in the locked state. You can unlock them using the \"unlock\" action. You must then wait for the tokens to finish unlocking before attempting this action again." );

    from_acnts.modify( from, owner, [&]( auto& a ) {
        a.balance -= value;
    });
}

void lumetokenctr::add_balance( name owner, asset value, name ram_payer )
{
    accounts to_acnts( _self, owner.value );
    auto to = to_acnts.find( value.symbol.code().raw() );
    if( to == to_acnts.end() ) {
        to_acnts.emplace( ram_payer, [&]( auto& a ) {
            a.balance = value;
            a.locked.amount = 0;
            a.locked.symbol = value.symbol;
        });
    } else {
        to_acnts.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
        });
    }
}

void lumetokenctr::open( name owner, const symbol& symbol, name ram_payer )
{
    require_auth( ram_payer );

    auto sym_code_raw = symbol.code().raw();

    stats statstable( _self, sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
    check( st.supply.symbol == symbol, "symbol precision mismatch" );

    accounts acnts( _self, owner.value );
    auto it = acnts.find( sym_code_raw );
    if( it == acnts.end() ) {
        acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = asset{0, symbol};
        });
    }
}

void lumetokenctr::close( name owner, const symbol& symbol )
{
    require_auth( owner );
    accounts acnts( _self, owner.value );
    auto it = acnts.find( symbol.code().raw() );
    check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
    check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
    acnts.erase( it );
}



EOSIO_DISPATCH( lumetokenctr, (create)(burn)(issue)(transfer)(open)(close)(lock)(unlock)(transferlock) )
