#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/time.hpp>
#include <eosio/system.hpp>

#include <string>
using namespace eosio;
using std::string;

CONTRACT lumetokenctr : public eosio::contract {

  using contract::contract;

  public:

    ACTION create   ( name issuer, asset maximum_supply );
    ACTION burn     ( name owner, asset quantity );
    ACTION issue    ( name to, asset quantity, string memo );
    ACTION transfer ( name from, name to, asset quantity, string memo );

    ACTION open     ( name owner, const symbol& symbol, name ram_payer );
    ACTION close    ( name owner, const symbol& symbol );
    ACTION lock     ( name owner, asset quantity, uint8_t days );
    ACTION unlock   ( name owner, asset quantity );

    ACTION transferlock ( name from, name to, asset quantity, string memo, uint64_t days );

    void convert_locked_to_balance( name owner );

    inline asset get_supply  ( symbol_code sym )const;
    inline asset get_balance ( name owner, symbol_code sym )const;

  private:
    const eosio::symbol LUMESYMBOL = symbol(symbol_code("LUME"), 3);

    TABLE account 
    {
        asset    balance;
        asset    locked;
        
        uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("accounts"), account> accounts;

    TABLE currency_stats 
    {
        asset    supply;
        asset    max_supply;
        name     issuer;

        uint64_t primary_key()const { return supply.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("stat"), currency_stats> stats;

    TABLE locked_fund 
    {
        uint64_t lock_time;
        asset    quantity;
        uint64_t primary_key()const { return lock_time; }
    };
    typedef eosio::multi_index<name("locked"), locked_fund > locked_funds;

    TABLE unlocking_fund 
    {
        uint64_t   id;
        time_point unlocked_at;
        asset      quantity;
        uint64_t   primary_key()const { return id; }
    };
    typedef eosio::multi_index<name("unlocking"), unlocking_fund > unlocking_funds;

    void sub_balance( name owner, asset value );
    void add_balance( name owner, asset value, name ram_payer );

};

asset lumetokenctr::get_supply( symbol_code sym )const
{
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );
    return st.supply;
}

asset lumetokenctr::get_balance( name owner, symbol_code sym )const
{
    accounts accountstable( _self, owner.value );
    const auto& ac = accountstable.get( sym.raw() );
    return ac.balance;
}
