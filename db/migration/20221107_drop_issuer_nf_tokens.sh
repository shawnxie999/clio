#!/bin/bash 
#Schema for issuer_nf_tokens table changed, with a new taxon column. Old table needs to be dropped

XLS20_PRIOR_LEDGER_INDEX=75443456

SEQUENCE_1=$(cqlsh -e "SELECT sequence FROM clio.ledger_range WHERE is_latest=true;")

sleep 10s

SEQUENCE_2=$(cqlsh -e "SELECT sequence FROM clio.ledger_range WHERE is_latest=true;")

if [[ $SEQUENCE_1 !=  $SEQUENCE_2 ]]
then
    echo "Clio is still running! Stop before running the script."
    exit 1
fi

cqlsh -e "INSERT INTO clio.ledger_range(is_latest, sequence) VALUES(true,$XLS20_PRIOR_LEDGER_INDEX);"

cqlsh -e "DROP TABLE clio.issuer_nf_tokens;"
