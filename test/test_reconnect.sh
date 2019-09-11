#!/bin/bash
export URL="http://localhost:81/App/TestApp?username=google&pageUrl=incorrect-url"
export CURL="curl"

for i in 0 1 2 3 4 5 6 7 8 9; do
    $CURL "$URL";
done
sudo service mysql stop
echo "mysql down"
for i in 0 1 2 3 4 5 6 7 8 9; do
    $CURL "$URL";
    sleep 1; 
done
sudo service mysql start
echo "mysql up"
for i in 0 1 2 3 4 5 6 7 8 9; do
    $CURL "$URL";
done
