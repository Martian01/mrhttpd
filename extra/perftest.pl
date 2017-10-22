#!/usr/bin/perl

# Copyright (c) 2007-2011  Martin Rogge <martin_rogge@users.sourceforge.net>

# This script is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This is a test script for volume performance testing.
# The output is a CSV file (CSV = comma separated values)
# that can be loaded in a spreadsheet like LibreOffice Calc.
#
# Start as root with port and server as parameter, like:
# 
# perl perftest.pl 80 darkstar.example.net

# Note: prior to testing concurrency levels above 1000 I need 
# to increase a number of system limits, like:
#
# echo 65536 > /proc/sys/net/ipv4/tcp_max_syn_backlog
# echo 65536 > /proc/sys/net/core/somaxconn
# echo 65536 > /proc/sys/net/core/netdev_max_backlog
# echo 16777216 > /proc/sys/net/core/rmem_max
# echo 16777216 > /proc/sys/net/core/wmem_max
# echo 4096 87380 16777216 > /proc/sys/net/ipv4/tcp_rmem
# echo 4096 16384 16777216 > /proc/sys/net/ipv4/tcp_wmem
# echo 16384 > /proc/sys/vm/min_free_kbytes
# ulimit -n 65536
# ulimit -i 65536
# ulimit -u 65536

die "Must be root to run benchmark, don\'t ask why." unless $< == 0;

$PORT = ($#ARGV >= 0) ? $ARGV[0] : "8000";
$HOST = ($#ARGV >= 1) ? $ARGV[1] : "localhost";

header();

benchmark("ab -q -c 1 -n 10000 http://$HOST:$PORT/cgi-bin/perftest.sh");
benchmark("ab -q -c 10 -n 10000 http://$HOST:$PORT/cgi-bin/perftest.sh");
benchmark("ab -q -c 100 -n 10000 http://$HOST:$PORT/cgi-bin/perftest.sh");
benchmark("ab -q -c 1000 -n 10000 http://$HOST:$PORT/cgi-bin/perftest.sh");

benchmark("ab -q -k -c 1 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -k -c 10 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -k -c 100 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -k -c 1000 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -k -c 10000 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -k -c 20000 -n 1000000 http://$HOST:$PORT/perftest.html");

benchmark("ab -q -c 1 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -c 10 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -c 100 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -c 1000 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -c 10000 -n 1000000 http://$HOST:$PORT/perftest.html");
benchmark("ab -q -c 20000 -n 1000000 http://$HOST:$PORT/perftest.html");

sub benchmark
{
  # declare and init local variables 8-)
  my $kernel=""; my $in_number=""; my $in_concurrency=""; my $in_keepalive=""; 
  my $server=""; my $host=""; my $port=""; my $path=""; my $length=""; my $concurrency=""; my $time=""; my $number=""; 
  my $failed=""; my $write_error=""; my $non_2xx=""; my $keepalive=""; 
  my $total=""; my $html=""; my $rps=""; my $tpr=""; my $tpcr=""; my $rate=""; 
  my $cmin=""; my $cmean=""; my $csd=""; my $cmed=""; my $cmax=""; 
  my $pmin=""; my $pmean=""; my $psd=""; my $pmed=""; my $pmax=""; 
  my $wmin=""; my $wmean=""; my $wsd=""; my $wmed=""; my $wmax=""; 
  my $tmin=""; my $tmean=""; my $tsd=""; my $tmed=""; my $tmax=""; 

  $in_keepalive = ($_[0] =~ /\s-k\s/) ? "yes" : "no";
  if ($_[0] =~ /\s-n\s+(\S+)/) { $in_number=$1; }
  if ($_[0] =~ /\s-c\s+(\S+)/) { $in_concurrency=$1; }

  foreach(`$_[0]`) {
    if (/^Server Software:\s+(\S+)/)                      { $server = $1; }
    if (/^Server Hostname:\s+(\S+)/)                      { $host = $1; }
    if (/^Server Port:\s+(\S+)/)                          { $port = $1; }
    if (/^Document Path:\s+(\S+)/)                        { $path = $1; }
    if (/^Document Length:\s+(\S+)/)                      { $length = $1; }
    if (/^Concurrency Level:\s+(\S+)/)                    { $concurrency = $1; }
    if (/^Time taken for tests:\s+(\S+)/)                 { $time = $1; }
    if (/^Complete requests:\s+(\S+)/)                    { $number = $1; }
    if (/^Failed requests:\s+(\S+)/)                      { $failed = $1; }
    if (/^Write errors:\s+(\S+)/)                         { $write_error = $1; }
    if (/^Non-2xx responses:\s+(\S+)/)                    { $non_2xx = $1; }
    if (/^Keep-Alive requests:\s+(\S+)/)                  { $keepalive = $1; }
    if (/^Total transferred:\s+(\S+)/)                    { $total = $1; }
    if (/^HTML transferred:\s+(\S+)/)                     { $html = $1; }
    if (/^Requests per second:\s+(\S+)/)                  { $rps = $1; }
    if (/^Time per request:\s+(\S+)\s+\[ms\]\s+\(mean\)/) { $tpr = $1; }
    if (/^Time per request:\s+(\S+)\s+\[ms\]\s+\(mean,/)  { $tpcr = $1; }
    if (/^Transfer rate:\s+(\S+)/)                        { $rate = $1; }

    if (/^Connect:\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/) { 
      $cmin  = $1; 
      $cmean = $2; 
      $csd   = $3; 
      $cmed  = $4; 
      $cmax  = $5; 
    }
    if (/^Processing:\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/) { 
      $pmin  = $1; 
      $pmean = $2; 
      $psd   = $3; 
      $pmed  = $4; 
      $pmax  = $5; 
    }
    if (/^Waiting:\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/) { 
      $wmin  = $1; 
      $wmean = $2; 
      $wsd   = $3; 
      $wmed  = $4; 
      $wmax  = $5; 
    }
    if (/^Total:\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)/) { 
      $tmin  = $1; 
      $tmean = $2; 
      $tsd   = $3; 
      $tmed  = $4; 
      $tmax  = $5; 
    }
  }
  $kernel = `uname -r`;
  chomp $kernel;

  sleep(5); # try and calm down SYN flood detection - seems not to work, though

  print "$kernel, $in_number, $in_concurrency, $in_keepalive, ",
        "$server, $host, $port, $path, $length, $concurrency, $time, $number, ",
        "$failed, $write_error, $non_2xx, $keepalive, ",
        "$total, $html, $rps, $tpr, $tpcr, $rate, ",
        "$cmin, $cmean, $csd, $cmed, $cmax, ",
        "$pmin, $pmean, $psd, $pmed, $pmax, ",
        "$wmin, $wmean, $wsd, $wmed, $wmax, ",
        "$tmin, $tmean, $tsd, $tmed, $tmax, ",
        "EOL\n"
}

sub header
{
  print "Kernel Version, In Number, In Concurrency, In Keep-Alive, ",
        "Server, Host, Port, Path, Length, Concurrency, Time, Number, ",
        "Failed, Write Errors, Non 2xx, Keep-Alive, ",
        "Total, HTML, Req. per sec, Time per req, Time per conc, Rate, ",
        "Conn Min, Conn Mean, Conn SD, Conn Median, Conn Max, ",
        "Proc Min, Proc Mean, Proc SD, Proc Median, Proc Max, ",
        "Wait Min, Wait Mean, Wait SD, Wait Median, Wait Max, ",
        "Total Min, Total Mean, Total SD, Total Median, Total Max, ",
        "Remark\n"
}
