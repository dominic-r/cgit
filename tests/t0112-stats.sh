#!/bin/sh

test_description='Check content on stats page'
. ./setup.sh

test_expect_success 'generate foo stats' 'cgit_url "foo/stats" >tmp'
test_expect_success 'show day stats table' '
	grep "Commits per author per day" tmp
'
test_expect_success 'show week stats table' '
	grep "Commits per author per week" tmp
'
test_expect_success 'show month stats table' '
	grep "Commits per author per month" tmp
'
test_expect_success 'show year stats table' '
	grep "Commits per author per year" tmp
'
test_expect_success 'show four stats tables by default' '
	test "$(grep -c "<table class=.stats.>" tmp)" = 4
'

test_done
