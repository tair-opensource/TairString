set testmodule [file normalize ../lib/tairstring_module.so]

start_server {tags {"ex_string"} overrides {bind 0.0.0.0}} {
    r module load $testmodule
    test {exset basic} {
        r del exstringkey

        catch {r exset exstringkey} err
        assert_match {*ERR*wrong*number*of*arguments*for*} $err

        catch {r exset exstringkey var xxxx} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var flags} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r exset exstringkey bar WITHVERSION]
        assert_equal $res 1

        set res [r exset exstringkey bar1]
        assert_equal $res "OK"

        set res [r exset exstringkey bar1 flags 10]
        assert_equal $res "OK"

        set res [r exget exstringkey]
        assert_equal $res "bar1 3"

        set res [r exget exstringkey withflags]
        assert_equal $res "bar1 3 10"

        set res [r exset exstringkey bar2 WITHVERSION]
        assert_equal $res 4

        set res [r exget exstringkey withflags]
        assert_equal $res "bar2 4 10"
    }

    test {exset XX/NX} {
        r del exstringkey

        set ret_val [r exset exstringkey foo XX]
        assert_equal "" $ret_val

        set ret_val [r exset exstringkey foo NX WITHVERSION]
        assert_equal 1 $ret_val

        set res [r exget exstringkey]
        assert_equal $res "foo 1"

        set ret_val [r exset exstringkey foo NX]
        assert_equal "" $ret_val
    }

    test {exset VER/ABS} {
        r del exstringkey

        catch {r exset exstringkey foo VER -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exset exstringkey foo ABS -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        set ret_val [r exset exstringkey foo VER 1]
        assert_equal "OK" $ret_val

        set res [r exget exstringkey]
        assert_equal $res "foo 1"

        set ret_val [r exset exstringkey foo VER 1 WITHVERSION]
        assert_equal 2 $ret_val

        set res [r exget exstringkey]
        assert_equal $res "foo 2"

        catch  {r exset exstringkey foo VER 1} err
        assert_match {*ERR*update*version*is*stale*} $err
       
        set ret_val [r exset exstringkey bar VER 2]
        assert_equal "OK" $ret_val

        set res [r exget exstringkey]
        assert_equal $res "bar 3"

        set ret_val [r exset exstringkey foo ABS 10 WITHVERSION]
        assert_equal 10 $ret_val

        set res [r exget exstringkey]
        assert_equal $res "foo 10"

        set res [r del exstringkey]
        assert_equal $res 1

        set res [r exset exstringkey foo ABS 25 WITHVERSION]
        assert_equal $res 25
    }

    test {exset EX/EXAT/PX/PXAT} {
        r del exstringkey

        catch {r exset exstringkey foo EX -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exset exstringkey foo PX -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exset exstringkey foo PXAT -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exset exstringkey foo EXAT -1} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey foo EXAT 0} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey foo EX 0} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        set ret_val [r exset exstringkey foo EX 2]
        assert_equal "OK" $ret_val

        set ret_val [r exists exstringkey]
        assert_equal 1 $ret_val
 
        after 4000

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set ret_val [r exset exstringkey foo PX 2000]
        assert_equal "OK" $ret_val

        set ret_val [r exists exstringkey]
        assert_equal 1 $ret_val

        after 4000

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val
    }

    test {exset KEEPTTL} {
        r del exstringkey

        set ret_val [r exset exstringkey foo EX 10]
        assert_equal "OK" $ret_val

        set ttl [r ttl exstringkey]
        assert {$ttl > 0 && $ttl <= 10}

        set ret_val [r exset exstringkey foo]
        assert_equal "OK" $ret_val

        set ttl [r ttl exstringkey]
        assert {$ttl == -1}

        set ret_val [r exset exstringkey foo EX 10]
        assert_equal "OK" $ret_val

        set ret_val [r exset exstringkey foo KEEPTTL]
        assert_equal "OK" $ret_val

        set ttl [r ttl exstringkey]
        assert {$ttl > 0 && $ttl <= 10}
    }

    test {unsupported flags} {
        r del exstringkey

        catch {r exset exstringkey foo EX 10 max 10} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exincrbyfloat exstringkey 1.0 def 300} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exincrbyfloat exstringkey 1.0 WITHVERSION} err
        assert_match {*ERR*syntax*error*} $err
    }

    test {exincrby basic} {
        r del exstringkey

        set res [r exset exstringkey aaa]
        assert_equal $res "OK"

        catch {r exincrby exstringkey 10} err
        assert_match {*ERR*value*is*not*an*integer*} $err

        set res [r del exstringkey]
        assert_equal $res 1
        
        catch {r exincrby exstringkey abc} err
        assert_match {*ERR*value*is*not*an*integer*} $err

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r exincrby exstringkey 100 def 200 WITHVERSION]
        assert_equal $res "200 1"

        set res [r exincrby exstringkey 108]
        assert_equal $res "308"

        set res [r exincrby exstringkey -208 def 300 WITHVERSION]
        assert_equal $res "100 3"

        catch {r exincrby exstringkey 123456789098765432123456789} err
        assert_match {*ERR*value*is*not*an*integer*} $err

        set res [r exget exstringkey]
        assert_equal $res "100 3"

        set res [r del exstringkey]
        assert_equal $res 1

        catch {r exincrby exstringkey 100 def 123456789098765432123456789} err
        assert_match {*ERR*value*is*not*an*integer*} $err

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r exincrby exstringkey 1 def -100 WITHVERSION]
        assert_equal $res "-100 1"

    }

    test {exincrby NX/XX} {
        r del exstringkey

        set res [r exincrby exstringkey 108 XX]
        assert_equal $res ""

        set res [r exincrby exstringkey 108 NX]
        assert_equal $res "108"

        set res [r exincrby exstringkey 108 NX]
        assert_equal $res ""
    }

    test {exincrby ver/abs} {
        r del exstringkey

        catch {r exincrby exstringkey 10 VER -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrby exstringkey 10 ABS -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        set res [r exincrby exstringkey 10 VER 1 WITHVERSION]
        assert_equal $res "10 1"

        set res [r exincrby exstringkey 10 VER 1 WITHVERSION]
        assert_equal $res "20 2"

        catch {r exincrby exstringkey 10 VER 1} err
        assert_match {*ERR*update*version*is*stale*} $err

        set res [r exincrby exstringkey 10 VER 2 WITHVERSION]
        assert_equal $res "30 3"

        set res [r exincrby exstringkey 10 ABS 101 WITHVERSION]
        assert_equal $res "40 101"

        set res [r del exstringkey]
        assert_equal $res 1

        set res [r exincrby exstringkey 12 def -16 ABS 103 WITHVERSION]
        assert_equal $res "-16 103"

        set res [r del exstringkey]
        assert_equal $res 1

        set res [r exincrby exstringkey 12 ABS 105 WITHVERSION]
        assert_equal $res "12 105"

        set res [r del exstringkey]
        assert_equal $res 1
    }

    test {exincrby ex/exat/px/pxat} {
        r del exstringkey

        catch {r exincrby exstringkey  10 EX -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrby exstringkey  10 EXAT -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrby exstringkey  10 PX -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrby exstringkey  10 PXAT -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        set res [r exincrby exstringkey 10 EX 2]
        assert_equal $res "10"

        set ret_val [r exists exstringkey]
        assert_equal 1 $ret_val

        after 4000

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r exincrby exstringkey 10 PX 2000]
        assert_equal $res "10"

        set ret_val [r exists exstringkey]
        assert_equal 1 $ret_val

        after 4000

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val
    }

    test {exincrby nonegative} {
        r del exstringkey

        set res [r exincrby a 100 WITHVERSION]
        assert_equal $res "100 1"
        
        set res [r exincrby a -200 nonegative WITHVERSION]
        assert_equal $res "0 2"
    }

    test {exincrbyfloat basic} {
        r del exstringkey

        catch {r exincrbyfloat exstringkey abc} err
        assert_match {*ERR*value*is*not*a*float*} $err

        set res [r exincrbyfloat exstringkey 108.13]
        assert_equal $res "108.13"
    }

    test {exincrbyfloat NX/XX} {
        r del exstringkey

        set res [r exincrbyfloat exstringkey 108.13 XX]
        assert_equal $res ""

        set res [r exincrbyfloat exstringkey 108.13 NX]
        assert_equal $res "108.13"

        set res [r exincrbyfloat exstringkey 108.13 NX]
        assert_equal $res ""
    }

    test {exincrbyfloat ver/abs} {
        r del exstringkey

        catch {r exincrbyfloat exstringkey 10.1 VER -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        set res [r exincrbyfloat exstringkey 10.1 VER 1]
        assert_equal $res "10.1"

        set res [r exincrbyfloat exstringkey 10.1 VER 1]
        assert_equal $res "20.2"

        catch {r exincrbyfloat exstringkey 10.1 VER 1} err
        assert_match {*ERR*update*version*is*stale*} $err

        set res [r exincrbyfloat exstringkey 10.1 VER 2]
        assert_equal $res "30.3"

		set res [r del exstringkey]
		assert_equal $res 1

		set res [r exincrbyfloat exstringkey 10.1 ABS 101]
		assert_equal $res "10.1"

		set res [r exget exstringkey]
		assert_equal $res "10.1 101"

		set res [r exincrbyfloat exstringkey -3 ABS 10]
		assert_equal $res "7.1"

		set res [r exget exstringkey]
		assert_equal $res "7.1 10"
    }

    test {exincrbyfloat ex/exat/px/pxat} {
        r del exstringkey

        catch {r exincrbyfloat exstringkey 10.1 EX -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrbyfloat exstringkey 10.1 EXAT -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrbyfloat exstringkey 10.1 PX -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        catch {r exincrbyfloat exstringkey 10.1 PXAT -1} err
        assert_match {*ERR*syntax*error*} $err

        set ret_val [r exists exstringkey ]
        assert_equal 0 $ret_val

        set res [r exincrbyfloat exstringkey 10.1 EX 2]
        assert_equal $res "10.1"

        set ret_val [r exists exstringkey]
        assert_equal 1 $ret_val

        after 4000

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r exincrbyfloat exstringkey 10.1 PX 2000]
        assert_equal $res "10.1"

        set ret_val [r exists exstringkey]
        assert_equal 1 $ret_val

        after 4000

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val
    }

    test {exappend basic} {
        r del exstringkey

        catch {r exappend exstringkey} err
        assert_match {*ERR*wrong*number*of*arguments*for*} $err

        catch {r exappend exstringkey NX 100} err
        assert_match {*ERR*syntax*error*} $err

        set res [r exappend exstringkey foo]
        assert_equal $res 1

        set res [r exget exstringkey]
        assert_equal $res "foo 1"

        set res [r exappend exstringkey bar]
        assert_equal $res 2

        set res [r exget exstringkey]
        assert_equal $res "foobar 2"
    }   

    test {exappend ver/abs} {
        r del exstringkey

        catch {r exappend exstringkey abs aaa} $err
        assert_match {*ERR*syntax*error*} $err

        set res [r exappend exstringkey foo VER 10]
        assert_equal $res 1

        set res [r exappend exstringkey bar VER 1]
        assert_equal $res 2

        catch {r exappend exstringkey foo VER 1} err
        assert_match {*ERR*update*version*is*stale*} $err

        set res [r exappend exstringkey my VER 2]
        assert_equal $res 3

        set res [r exappend exstringkey Value ABS 10]
        assert_equal $res 10

        set res [r exappend exstringkey value VER 10]
        assert_equal $res 11

        set res [r del exstringkey]
        assert_equal $res 1

        set res [r exappend exstringkey value ABS 100]
        assert_equal $res 100
    }

    test {exappend NX/XX} {
        r del exstringkey

        set res [r exappend exstringkey foo XX]
        assert_equal $res ""

        set res [r exappend exstringkey foo NX]
        assert_equal $res 1

        set res [r exappend exstringkey bar NX]
        assert_equal $res ""

        set res [r exappend exstringkey bar XX]
        assert_equal $res 2
    }

    test {exprepend basic} {
        r del exstringkey

        set res [r exprepend exstringkey foo]
        assert_equal $res 1

        set res [r exget exstringkey]
        assert_equal $res "foo 1"

        set res [r exprepend exstringkey bar]
        assert_equal $res 2

        set res [r exget exstringkey]
        assert_equal $res "barfoo 2"
    }

    test {exprepend ver/abs} {
        r del exstringkey

        set res [r exprepend exstringkey foo VER 10]
        assert_equal $res 1

        set res [r exprepend exstringkey bar VER 1]
        assert_equal $res 2

        catch {r exprepend exstringkey foo VER 1} err
        assert_match {*ERR*update*version*is*stale*} $err

        set res [r exprepend exstringkey my VER 2]
        assert_equal $res 3

        set res [r exprepend exstringkey Value ABS 10]
        assert_equal $res 10

        set res [r exprepend exstringkey value VER 10]
        assert_equal $res 11

        set res [r del exstringkey]
        assert_equal $res 1

        set res [r exprepend exstringkey value ABS 100]
        assert_equal $res 100
    }

    test {exprepend NX/XX} {
        r del exstringkey

        set res [r exprepend exstringkey foo XX]
        assert_equal $res ""

        set res [r exprepend exstringkey foo NX]
        assert_equal $res 1

        set res [r exprepend exstringkey bar NX]
        assert_equal $res ""

        set res [r exprepend exstringkey bar XX]
        assert_equal $res 2
    }

    test {exappend/exprepend} {
        r del exstringkey1
        r del exstringkey2

        set res [r exappend exstringkey1 x]
        assert_equal $res 1

        set res [r exprepend exstringkey2 y]
        assert_equal $res 1

        set res [r exappend exstringkey1 z]
        assert_equal $res 2

        set res [r exget exstringkey1]
        assert_equal $res "xz 2"

        set res [r exget exstringkey2]
        assert_equal $res "y 1"

        set res [r del exstringkey1]
        assert_equal $res 1

        set res [r del exstringkey2]
        assert_equal $res 1

        set res [r exprepend exstringkey1 x]
        assert_equal $res 1

        set res [r exappend exstringkey2 y]
        assert_equal $res 1

        set res [r exprepend exstringkey1 z]
        assert_equal $res 2

        set res [r exget exstringkey1]
        assert_equal $res "zx 2"

        set res [r exget exstringkey2]
        assert_equal $res "y 1"
        
        set res [r del exstringkey1]
        assert_equal $res 1

        set res [r del exstringkey2]
        assert_equal $res 1
    }

    test {exgae} {
        r del exstringkey

        catch {r exgae exstringkey } err
        assert_match {*ERR*wrong*number*of*arguments*for*} $err

        set res [r exgae exstringkey ex 2]
        assert_equal $res ""

        set res [r exset exstringkey foo ex 2 flags 233 WITHVERSION]
        assert_equal $res 1

        set res [r exgae exstringkey ex 4]
        assert_equal $res "foo 1 233"

        after 1500

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 1

        after 1500

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 1

        after 3000

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 0

        set res [r exget exstringkey]
        assert_equal $res ""



        set res [r exgae exstringkey px 2000]
        assert_equal $res ""

        set res [r exset exstringkey foo px 2000 flags 233 WITHVERSION]
        assert_equal $res 1

        set res [r exgae exstringkey px 4000]
        assert_equal $res "foo 1 233"

        after 1500

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 1

        after 1500

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 1

        after 3000

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 0

        set res [r exget exstringkey]
        assert_equal $res ""

        set res [r exset exstringkey foo ]
        assert_equal $res "OK"

        set expat [expr [clock seconds] + 2]
        set res [r exgae exstringkey exat $expat]
        assert_equal $res "foo 1 0"

        after 3000

        set ret_val [r exists exstringkey]
        assert_equal $ret_val 0
    }

    test {exset/exget/exincrby syntax error} {
        r del exstringkey

        r exset exstringkey var

        catch {r exset exstringkey var EX} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var EX 10 EX 15} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var EX 10 PX 20} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var EX 10 EXAT 20000000} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var PX 2000 PXAT 998244353} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var EX 10 PXAT 998244353} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var NX XX} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var VER 1 ABS 2} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var FLAGS 3 FLAGS 4} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var DEF 100} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var MIN 20 MAX 40} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exset exstringkey var WITHFLAGS} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exget exstringkey WITHFLAGS MIN 10} err
        assert_match {*ERR*wrong*number*of*arguments*} $err

        catch {r exget exstringkey PX 2000} err
        assert_match {*ERR*wrong*number*of*arguments*} $err

        catch {r exget exstringkey NX} err
        assert_match {*ERR*syntax*error*} $err

        r exset exstringkey 1

        catch {r exincrby exstringkey 1 MIN 10 MIN 20} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exincrby exstringkey 1 MAX 10 MAX 20} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exincrby exstringkey 1 NX XX} err
        assert_match {*ERR*syntax*error*} $err

        catch {r exincrby exstringkey 1 WITHFLAGS} err
        assert_match {*ERR*syntax*error*} $err
    }

    test {exsetver} {
        r del exstringkey

        set res [r exset exstringkey bar]
        assert_equal $res "OK"

        set res [r exget exstringkey]
        assert_equal $res "bar 1"

        set res [r exsetver exstringkey 100]
        assert_equal $res "1"

        set res [r exget exstringkey]
        assert_equal $res "bar 100"

        catch {r exsetver exstringkey abc} err
        assert_match {*ERR*syntax*error*} $err

         catch {r exsetver exstringkey -1} err
        assert_match {*ERR*syntax*error*} $err
    }

    test {exstring rdb} {
        r del exstringkey

        set res [r exset exstringkey bar WITHVERSION]
        assert_equal $res 1

        set res [r exset exstringkey bar2 WITHVERSION]
        assert_equal $res 2

        r bgsave
        waitForBgsave r
        r debug reload

        set res [r exget exstringkey]
        assert_equal $res "bar2 2"
    }

    test {exstring aof} {
        r del exstringkey
        r del exstringkey2
        r config set aof-use-rdb-preamble no

        set res [r exset exstringkey bar abs 100 WITHVERSION]
        assert_equal $res 100
        set res [r exset exstringkey2 bar flags 10]
        assert_equal $res "OK"

        set res [r exget exstringkey]
        assert_equal $res "bar 100"

        r bgrewriteaof
        waitForBgrewriteaof r
        r debug loadaof

        set res [r exget exstringkey]
        assert_equal $res "bar 100"
        set res [r exget exstringkey2 WITHFLAGS]
        assert_equal $res "bar 1 10"
    }

    test {exstring type} {
        r del exstringkey

        set res [r exset exstringkey bar abs 100 WITHVERSION]
        assert_equal $res 100

        set res [r type exstringkey]
        assert_equal $res "exstrtype"
    }

    test {exincrby with boundary} {
        r del exstringkey

        catch {r exincrby exstringkey 10000 max 200} err
        assert_match {ERR*increment*or*decrement*would*overflow} $err

        set res [r exincrby exstringkey 108 WITHVERSION]
        assert_equal $res "108 1"

        set res [r exincrby exstringkey -1 WITHVERSION]
        assert_equal $res "107 2"

        catch {r exincrby exstringkey -10 min 100} err
        assert_match {ERR*increment*or*decrement*would*overflow} $err

        catch {r exincrby exstringkey -10 min 100 max 10} err
        assert_match {ERR*min*or*max*is*specified*but*not*valid} $err

        catch {r exincrby exstringkey abc} err
        assert_match {ERR*value*is*not*an*integer} $err

        # add a value larger than LLONG_MAX
        catch {r exincrby exstringkey 9223372036854775808} err
        assert_match {ERR*value*is*not*an*integer} $err

        # add LLONG_MAX which will overflow, verify if return the expected error
        catch {r exincrby exstringkey 9223372036854775807} err
        assert_match {ERR*increment*or*decrement*would*overflow} $err

        set res [r exincrby exstringkey -200 WITHVERSION]
        assert_equal $res "-93 3"

        # add LLONG_MIN which will overflow, verify if return the expected error
        catch {r exincrby exstringkey -9223372036854775807} err
        assert_match {ERR*increment*or*decrement*would*overflow} $err
    }

    test {exincrbyfloat with boundary} {
        r del exstringkey

        catch {r exincrbyfloat exstringkey 10000.12 max 200.12} err
        assert_match {ERR*increment*or*decrement*would*overflow} $err

        set res [r exincrbyfloat exstringkey 108.123]
        assert_equal $res "108.123"

        set res [r exget exstringkey]
        assert_equal $res "108.123 1"

        set res [r exincrbyfloat exstringkey -1]
        assert_equal $res "107.123"

        set res [r exget exstringkey]
        assert_equal $res "107.123 2"

        catch {r exincrbyfloat exstringkey -10.12 min 100.12} err
        assert_match {ERR*increment*or*decrement*would*overflow} $err

        catch {r exincrbyfloat exstringkey -10 min 100.12 max 10.12} err
        assert_match {ERR*min*or*max*is*specified*but*not*valid} $err

        catch {r exincrbyfloat exstringkey abc} err
        assert_match {ERR*value*is*not*a*float} $err
    }

    test {exset ignore version} {
        r del exstringkey

        set res [r exset exstringkey bar]
        assert_equal $res "OK"

        set res [r exget exstringkey ]
        assert_equal $res "bar 1"

        catch {r exset exstringkey bar1 VER 10} err
        assert_match {*ERR*update*version*is*stale*} $err

        set res [r exset exstringkey bar1 VER 0]
        assert_equal $res "OK"

        set res [r exget exstringkey ]
        assert_equal $res "bar1 2"
    }
	
    test {excas test functionality} {
        r del exstringkey

        set res [r excas exstringkey bar1 1]
        assert_equal $res -1

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r exset exstringkey bar]
        assert_equal $res "OK"

        set res [r exget exstringkey ]
        assert_equal $res "bar 1"

        set res [r excas exstringkey bar1 1]
        assert_equal $res "OK {} 2"

        set err [r excas exstringkey bar1 1]
        assert_match {*CAS_FAILED*} $err

        catch {r excas exstringkey bar1 -1} err
        assert_match {*ERR*syntax*error*} $err
    }

    test {excas test param num} {
        r del exstringkey

        catch {r excas exstringkey bar1} err
        assert_match {*ERR*wrong*number*of*arguments*} $err
    }

    test {excas test expire} {
        r del exstringkey

        set res [r exset exstringkey bar]
        assert_equal $res "OK"

        set res [r exget exstringkey ]
        assert_equal $res "bar 1"

        set res [r excas exstringkey bar1 1 EX 2]
        assert_equal $res "OK {} 2"

        set res [r exget exstringkey ]
        assert_equal $res "bar1 2"

        after 4000

        set res [r exget exstringkey ]
        assert_equal $res "" 
    }

    test {excad test works} {
        r del exstringkey

        r exset exstringkey bar1

        set res [r excad exstringkey 1]
        assert_equal $res "1"

        set res [r excad exstringkey 1]
        assert_equal $res "-1"

        r exset exstringkey bar1
        set res [r excad exstringkey 2]
        assert_equal $res "0"
    }

    test {cas test functionality} {
        r del exstringkey

        catch {r cas exstringkey bar1} err
        assert_match {*ERR*wrong*number*of*arguments*} $err

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r cas exstringkey bar1 bar2]
        assert_equal $res -1

        set ret_val [r exists exstringkey]
        assert_equal 0 $ret_val

        set res [r set exstringkey bar1]
        assert_equal $res "OK"

        set res [r get exstringkey ]
        assert_equal $res "bar1"

        set res [r cas exstringkey bar1 bar2]
        assert_equal $res 1

        set res [r get exstringkey ]
        assert_equal $res "bar2"

        set res [r cas exstringkey bar2 bar3]
        assert_equal $res 1

        set res [r get exstringkey ]
        assert_equal $res "bar3"

        set res [r cas exstringkey bar3 bar4 EX 2]
        assert_equal $res 1

        set res [r get exstringkey ]
        assert_equal $res "bar4"

        after 4000

        set res [r get exstringkey ]
        assert_equal $res ""

        set res [r set exstringkey bar1]
        assert_equal $res "OK"

        set res [r cas exstringkey bar1 bar2 PX 2000]
        assert_equal $res 1

        set res [r get exstringkey ]
        assert_equal $res "bar2"

        after 4000

        set res [r get exstringkey ]
        assert_equal $res ""
    }

     test {cad test functionality} {
        r del exstringkey

        set res [r set exstringkey bar1]
        assert_equal $res "OK"

        set res [r get exstringkey ]
        assert_equal $res "bar1"

        set res [r cad exstringkey bar2 ]
        assert_equal $res 0

        set res [r cad exstringkey bar1 ]
        assert_equal $res 1

        set res [r cad notexists bar1 ]
        assert_equal $res -1
    }

    test {exset dump/restore} {
        r del exstringkey

        set res [r exset exstringkey bar]
        assert_equal $res "OK"

        set dump [r dump exstringkey]
        r del exstringkey

        assert_equal "OK" [r restore exstringkey 0 $dump]
        assert_equal {bar 1} [r exget exstringkey]
    }
}

start_server {tags {"ex_string_repl"}} {
    r module load $testmodule
    r set myexkey foo

    start_server {} {
        r module load $testmodule

        test {Second server should have role master at first} {
            s role
        } {master}

        test {SLAVEOF should start with link status "down"} {
            r slaveof [srv -1 host] [srv -1 port]
            s master_link_status
        } {down}

        wait_for_sync r
        test {The link status should be up} {
            s master_link_status
        } {up}

        test {Sync should have transferred keys from master} {
            r get myexkey
        } {foo}

        test {KEEPTTL option should retain in replcia} {
            r -1 del myexkey
            assert {[r -1 exset myexkey foo ex 100] eq {OK}}
            set mttl [r -1 ttl myexkey]
            assert {$mttl != -1 && $mttl != -2}
            # Wait propagation
            after 1000
            set sttl [r ttl myexkey]
            assert {$sttl != -1 && $sttl != -2}

            assert {[r -1 exset myexkey foo2 keepttl] eq {OK}}
            set mttl [r -1 ttl myexkey]
            assert {$mttl != -1 && $mttl != -2}
            after 2000
            set sttl [r ttl myexkey]
            assert {$sttl != -1 && $sttl != -2}
        }
    }
}

start_server {tags {"exhash repl"} overrides {bind 0.0.0.0}} {
    r module load $testmodule
    set slave [srv 0 client]
    set slave_host [srv 0 host]
    set slave_port [srv 0 port]
    set slave_log [srv 0 stdout]

    start_server { overrides {bind 0.0.0.0}} {
        r module load $testmodule
        set master [srv 0 client]
        set master_host [srv 0 host]
        set master_port [srv 0 port]

        $slave slaveof $master_host $master_port

        wait_for_condition 50 100 {
            [lindex [$slave role] 0] eq {slave} &&
            [string match {*master_link_status:up*} [$slave info replication]]
        } else {
            fail "Can't turn the instance into a replica"
        }

        test {exset master-slave} {
            $master del exstringkey

            set res [$master exset exstringkey bar WITHVERSION]
            assert_equal $res 1

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "bar 1"
            
            set res [$master excas exstringkey bar1 1]
            assert_equal $res "OK {} 2"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "bar1 2"

            set res [$master excad exstringkey 2]
            assert_equal $res "1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res ""
        }

        test {exincrby master-slave} {
            $master del exstringkey

            set res [$master exincrby exstringkey 108 WITHVERSION]
            assert_equal $res "108 1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "108 1"
        }

        test {exsetver master-slave} {
            $master del exstringkey

            set res [$master exset exstringkey bar WITHVERSION]
            assert_equal $res 1

            set res [$master exget exstringkey]
            assert_equal $res "bar 1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "bar 1"

            set res [$master exsetver exstringkey 100]
            assert_equal $res "1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "bar 100"

            $master del exstringkey

            set res [$master exset exstringkey bar VER 1]
            assert_equal $res "OK"

            set res [$master exset exstringkey bar VER 1 WITHVERSION]
            assert_equal $res 2

            set res [$master exset exstringkey bar VER 2 WITHVERSION]
            assert_equal $res 3

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "bar 3"
        } 

        test {exappend/exprepend master-slave} {
            $master del exstringkey

            $master WAIT 1 5000

            set res [$master exappend exstringkey bar]
            assert_equal $res 1

            set res [$master exget exstringkey]
            assert_equal $res "bar 1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "bar 1"

            set res [$master exprepend exstringkey foo]
            assert_equal $res 2

            set res [$master exget exstringkey]
            assert_equal $res "foobar 2"

            $master WAIT 1 5000

            set res [$master exprepend exstringkey gao]
            assert_equal $res 3

            set res [$master exget exstringkey]
            assert_equal $res "gaofoobar 3"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res "gaofoobar 3"
        }

        test {exgae master-slave} {
            $master del exstringkey

            set res [$master exset exstringkey bar]
            assert_equal $res "OK"

            set res [$master exgae exstringkey ex 2]
            assert_equal $res "bar 1 0"

            $master WAIT 1 1000

            set res [$slave exget exstringkey]
            assert_equal $res "bar 1"

            after 4000

            set res [$slave exget exstringkey]
            assert_equal $res ""
        }

        test {cas test functionality master-slave} {
            $master del exstringkey

            $master WAIT 1 5000

            set res [$master set exstringkey bar1]
            assert_equal $res "OK"

            set res [$master get exstringkey ]
            assert_equal $res "bar1"

            $master WAIT 1 5000

            set res [$slave get exstringkey ]
            assert_equal $res "bar1"

            set res [$master cas exstringkey bar1 bar2]
            assert_equal $res 1

            set res [$master get exstringkey ]
            assert_equal $res "bar2"

            $master WAIT 1 5000

            set res [$slave get exstringkey ]
            assert_equal $res "bar2"

            set res [$master cas exstringkey bar2 bar3 EX 3]
            assert_equal $res 1

            $master WAIT 1 5000

            set res [$slave get exstringkey ]
            assert_equal $res "bar3"

            after 4000

            set res [$slave get exstringkey ]
            assert_equal $res ""
        }

        test {cad test functionality master-slave} {
            $master del exstringkey

            set res [$master set exstringkey bar1]
            assert_equal $res "OK"

            $master WAIT 1 5000

            set res [$slave get exstringkey ]
            assert_equal $res "bar1"

            set res [$master cad exstringkey bar1 ]
            assert_equal $res 1

            $master WAIT 1 5000

            set res [$slave get exstringkey ]
            assert_equal $res ""
        }  

        test {excas test functionality master-slave} {
            $master del exstringkey

            set res [$master exset exstringkey bar]
            assert_equal $res "OK"

            set res [$master exget exstringkey ]
            assert_equal $res "bar 1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey ]
            assert_equal $res "bar 1"

            set res [$master excas exstringkey bar1 1]
            assert_equal $res "OK {} 2"

            $master WAIT 1 5000

            set res [$slave exget exstringkey ]
            assert_equal $res "bar1 2"
        }

        test {excas test expire master-slave} {
            $master del exstringkey

            set res [$master exset exstringkey bar]
            assert_equal $res "OK"

            set res [$master exget exstringkey ]
            assert_equal $res "bar 1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey ]
            assert_equal $res "bar 1"

            set res [$master excas exstringkey bar1 1 EX 2]
            assert_equal $res "OK {} 2"

            $master WAIT 1 5000

            set res [$slave exget exstringkey ]
            assert_equal $res "bar1 2"
            
            after 4000

            set res [$slave exget exstringkey ]
            assert_equal $res ""
        }

        test {excad test works master-slave} {
            $master del exstringkey

            $master exset exstringkey bar1

            $master WAIT 1 5000

            set res [$slave exget exstringkey ]
            assert_equal $res "bar1 1"

            set res [$master excad exstringkey 1]
            assert_equal $res "1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey ]
            assert_equal $res ""
        }
        
        test {exset with flags master-slave} {
            $master del exstringkey

            set res [$master exset exstringkey bar FLAGS 10 WITHVERSION]
            assert_equal $res 1

            $master WAIT 1 5000

            set res [$slave exget exstringkey WITHFLAGS]
            assert_equal $res "bar 1 10"

            set res [$master excas exstringkey bar1 1]
            assert_equal $res "OK {} 2"

            $master WAIT 1 5000

            set res [$slave exget exstringkey WITHFLAGS]
            assert_equal $res "bar1 2 10"

            set res [$master excad exstringkey 2]
            assert_equal $res "1"

            $master WAIT 1 5000

            set res [$slave exget exstringkey]
            assert_equal $res ""
        }
 }
}
