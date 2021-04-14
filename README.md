# TairString

## Introduction  [中文说明](README-CN.md)
TairString is a redis module developed and open sourced by the Alibaba Tair team, which mainly contains two functions:

- Enhance the function of redis string, and add two CAS/CAD commands
- The new data type `exstrtype` is added. Compared with the redis string, the version can be specified on its value, so that functions such as distributed locks can be easily implemented. At the same time, flags can be set on the value to support the memcached protocol.

<br/>

At the same time, we have also open sourced an enhanced hash structure, which can set the expiration time and version number for the field. For details, please refer to [here](https://github.com/alibaba/TairHash)

<br/>

# CAS/CAD - Redis string enhancement

### CAS

#### Grammar and complexity：

> CAS <Key> <oldvalue> <newvalue> [EX seconds][exat timestamp] [PX milliseconds][pxat timestamp]  
> time complexity: O(1)

#### Command description:

> CAS（Compare And Set），Set its value to newvalue when the current value of the string corresponding to key is equal to oldvalue

#### Parameter Description:

> **key**: The key used to locate the string    
> **oldvalue**: Only when the current value of string and oldvalue are equal can its value be modified    
> **newvalue**：Set the value to newvalue when the current value of string   and oldvalue are equal  
> **EX**：Relative expiration time in seconds      
> **EXAT**：Absolute expiration time in seconds    
> **PX**：Relative expiration time in milliseconds    
> **PXAT**：Millisecond absolute expiration time   

#### Return value：

> Type：Long    
> Returns 1 if the value is successfully updated, -1 if the key does not exist, and 0 if the value fails to be updated   

#### Usage example：

```shell
127.0.0.1:6379> SET foo bar
OK
127.0.0.1:6379> CAS foo baa bzz
(integer) 0
127.0.0.1:6379> GET foo
"bar"
127.0.0.1:6379> CAS foo bar bzz
(integer) 1
127.0.0.1:6379> GET foo
"bzz"
127.0.0.1:6379> CAS foo bzz too EX 10
(integer) 1
127.0.0.1:6379> GET foo
"too"
127.0.0.1:6379> debug sleep 10
OK
(10.00s)
127.0.0.1:6379> GET foo
(nil)
127.0.0.1:6379>
```

### CAD

#### Grammar and complexity：

> CAD \<key\> \<value\>  
> time complexity: O(1)

#### Command description：

> CAD（Compare And Delete），Delete the Key when the value is equal to the value in the engine  

#### Parameter Description：

> **key**: The key used to locate the string      
> **value**: Delete only when the current value of string and value are equal  

#### Return value：

> Type：Long  
> Return 1 if the key is successfully deleted, -1 if the key does not exist, and 0 if the key is deleted

#### Usage example：

```shell
127.0.0.1:6379> SET foo bar
OK
127.0.0.1:6379> CAD foo bzz
(integer) 0
127.0.0.1:6379> CAD not-exists xxx
(integer) -1
127.0.0.1:6379> CAD foo bar
(integer) 1
127.0.0.1:6379> GET foo
(nil)
```

<br/>

# exstrtype - A String with version and compatible memcached protocol

## Quick start

```shell
127.0.0.1:6379> EXSET foo 100
OK
127.0.0.1:6379> EXGET foo
1) "100"
2) (integer) 1
127.0.0.1:6379> EXSET foo 200 VER 1
OK
127.0.0.1:6379> EXGET foo
1) "200"
2) (integer) 2
127.0.0.1:6379> EXSET foo 300 VER 1
(error) ERR update version is stale
127.0.0.1:6379> EXINCRBY foo 100
(integer) 300
127.0.0.1:6379> EXGET foo
1) "300"
2) (integer) 3
127.0.0.1:6379> EXSETVER foo 100
(integer) 1
127.0.0.1:6379> EXGET foo
1) "300"
2) (integer) 100
127.0.0.1:6379> EXCAS foo 400 100
1) OK
2)
3) (integer) 101
127.0.0.1:6379> EXGET foo
1) "400"
2) (integer) 101
127.0.0.1:6379> EXCAD foo 101
(integer) 1
127.0.0.1:6379> EXGET foo
(nil)
```

## Command introduction

## Command overview

| Command         |Grammar                                                                                                                                                                             | Details                                                                                                              |
| ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| EXSET         | EXSET \<key\> \<value\> [EX time][px time] [EXAT time][pxat time] [NX &#124; XX][ver version &#124; abs version] [FLAGS flags][withversion]                                      | Save the value to the key. The meaning of each parameter is explained later                              |
| EXGET         | EXGET \<key\> [WITHFLAGS]                                                                                                                                                        | Return the value and version of TairString                                      |
| EXSETVER      | EXSETVER \<key\> \<version\>                                                                                                                                                     | Set the version directly to a key, which is equivalent to EXSET ABS                                                                 |
| EXINCRBY      | EXINCRBY \<key\> \<num\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx &#124; xx] [VER version &#124; ABS version][min minval] [MAX maxval][nonegative] [WITHVERSION] | Auto-increment or decrement the Key                             |
| EXINCRBYFLOAT | EXINCRBYFLOAT \<key\> \<num\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx &#124; xx] [VER version &#124; ABS version][min minval] [MAX maxval]                      | Do the increment and decrement operations on Key, and the range of num is double                                   |
| EXCAS         | EXCAS \<key\> \<newvalue\> \<version\>                                                                                                                                           | Specify version to update the value. The update is successful when the version in the engine is the same as the specified one. If it fails, the old value and version will be returned      |
| EXCAD         | EXCAD \<key\> \<version\>                                                                                                                                                        | Delete the Key when the specified version is equal to the version in the engine, otherwise it will fail                                |
| EXAPPEND      | EXAPPEND \<key\> \<value\> [NX\|XX][ver version \| abs version]                                                                                                                  | Append string to key|
| EXPREPEND     | EXPREPEND \<key\> \<value\> [NX\|XX][ver version \| abs version]                                                                                                                 | Perform string prepend operation on key|
| EXGAE         | EXGAE \<key\> [EX time][px time] [EXAT time][pxat time] | GAE(Get And Expire),Return the value+version+flags of TairString, and set the expire of the key. **This command will not increase version** |
|               |||

<br/>

## EXSET

Grammar and complexity：

> EXSET \<key\> \<value\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx | xx] [VER version | ABS version][flags flags] [WITHVERSION]  
> time complexity：O(1)

Command description：  
> Save the value to the key

Parameter Description：  
> **key**: The key used to locate the string      
> **value**: The value of TairString to be set  
> **EX**：Relative expiration time in seconds      
> **EXAT**：Absolute expiration time in seconds    
> **PX**：Relative expiration time in milliseconds    
> **PXAT**：Millisecond absolute expiration time   
> **NX**：Write when data does not exist  
> **XX**：Write when data exists  
> **VER**：Version number, if the data exists, compare it with the version number of the existing data, if it is equal, write it, and add 1 to the version number; if it is not equal, return an error; if the data does not exist, ignore the incoming version number and write After the import is successful, the data version number becomes 1  
> **ABS**：Absolute version number, regardless of whether the data exists, overwrite the specified version number 
> **FLAGS**：The type is uint32_t to support the memcached protocol. If UINT_MAX is exceeded, an error will be returned. The default value is 0 by default  
> **WITHVERSION**：Modify the return value to version instead of "OK"  
 
Return value:   
> Type：String    
> Succuss return OK    

Usage example:
```shell
127.0.0.1:6379> EXSET foo bar XX
(nil)
127.0.0.1:6379> EXSET foo bar NX
OK
127.0.0.1:6379> EXSET foo bar NX
(nil)
127.0.0.1:6379> EXGET foo
1) "bar"
2) (integer) 1
127.0.0.1:6379> EXSET foo bar1 VER 10
(error) ERR update version is stale
127.0.0.1:6379> EXSET foo bar1 VER 1
OK
127.0.0.1:6379> EXGET foo
1) "bar1"
2) (integer) 2
127.0.0.1:6379> EXSET foo bar2 ABS 100
OK
127.0.0.1:6379> EXGET foo
1) "bar2"
2) (integer) 100
127.0.0.1:6379>
```

## EXGET

Grammar and complexity：

> EXGET \<key\> [WITHFLAGS]  
> time complexity：O(1)  

Command description：  
> return value + version  

Parameter Description：   
> **key**: The key used to locate the string
> **WITHFLAGS**: return flags  

Return value:   

> Type：List<String>/List<byte[]>  
> Success：value+version  

Usage example：
```shell
127.0.0.1:6379> EXSET foo bar ABS 100
OK
127.0.0.1:6379> EXGET foo
1) "bar"
2) (integer) 100
127.0.0.1:6379> DEL foo
(integer) 1
127.0.0.1:6379> EXGET foo
(nil)
127.0.0.1:6379>
```

## EXSETVER

Grammar and complexity：

> EXSETVER \<key\> \<version\>  
> time complexity：O(1)  

Command description：
> Set the version directly to a key, similar to EXSET ABS
 
Parameter Description：  
> **key**: The key used to locate the string   
> **version**: Version number

Return value：
> Type：Long  
> Success：1  


Usage example：

```shell
127.0.0.1:6379> EXSET foo bar
OK
127.0.0.1:6379> EXGET foo
1) "bar"
2) (integer) 1
127.0.0.1:6379> EXSETVER foo 2
(integer) 1
127.0.0.1:6379> EXGET foo
1) "bar"
2) (integer) 2
127.0.0.1:6379> EXSETVER not-exists 0
(integer) 0
127.0.0.1:6379>
```

## EXINCRBY

Grammar and complexity：
> EXINCRBY | EXINCRBY \<key\> \<num\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx | xx] [VER version | ABS version][min minval] [MAX maxval][nonegative] [WITHVERSION]
> time complexity：O(1)

Command description：  
> Perform auto-increment and auto-decrement operations on Key, and the range of type is long

Parameter Description：
> **key**: The key used to locate the string      
> **value**: Increments the number stored at key by value
> **EX**：Relative expiration time in seconds      
> **EXAT**：Absolute expiration time in seconds    
> **PX**：Relative expiration time in milliseconds    
> **PXAT**：Millisecond absolute expiration time   
> **NX**：Write when data does not exist  
> **XX**：Write when data exists  
> **VER**：Version number, if the data exists, compare it with the version number of the existing data, if it is equal, write it, and add 1 to the version number; if it is not equal, return an error; if the data does not exist, ignore the incoming version number and write After the import is successful, the data version number becomes 1  
> **ABS**：Absolute version number, regardless of whether the data exists, overwrite the specified version number 
> **FLAGS**：The type is uint32_t to support the memcached protocol. If UINT_MAX is exceeded, an error will be returned. The default value is 0 by default  
> **WITHVERSION**：Modify the return value to version instead of "OK" 
> **MIN**：The minimum value of TairString
> **MAX**：Maximum value of TairString
> **NONEGATIVE**：After setting, if the result of incrby is less than 0, set value to 0
> **WITHVERSION**：return cur version number

Return value：
> Type：Long  
> Success：return value

Usage example：

```shell
127.0.0.1:6379> EXINCRBY foo 100
(integer) 100
127.0.0.1:6379> EXINCRBY foo 100 MAX 150
(error) ERR increment or decrement would overflow
127.0.0.1:6379> FLUSHALL
OK
127.0.0.1:6379> EXINCRBY foo 100
(integer) 100
127.0.0.1:6379> EXINCRBY foo 100 MAX 150
(error) ERR increment or decrement would overflow
127.0.0.1:6379> EXINCRBY foo 100 MAX 300
(integer) 200
127.0.0.1:6379> EXINCRBY foo 100 MIN 500
(error) ERR increment or decrement would overflow
127.0.0.1:6379> EXINCRBY foo 100 MIN 500 MAX 100
(error) ERR min or max is specified, but not valid
127.0.0.1:6379> EXINCRBY foo 100 MIN 50
(integer) 300
127.0.0.1:6379>
```

## EXINCRBYFLOAT

Grammar and complexity：

> EXINCRBYFLOAT <key> <num> [EX time][px time] [EXAT time][exat time] [PXAT time][nx | xx] [VER version | ABS version][min minval] [MAX maxval]  
> time complexity：O(1)

Command description：
> Perform auto-increment and auto-decrement operations on Key, and the range of type is double

Parameter Description：  
> **key**: The key used to locate the string      
> **value**: Increments the number stored at key by value
> **EX**：Relative expiration time in seconds      
> **EXAT**：Absolute expiration time in seconds    
> **PX**：Relative expiration time in milliseconds    
> **PXAT**：Millisecond absolute expiration time   
> **NX**：Write when data does not exist  
> **XX**：Write when data exists  
> **VER**：Version number, if the data exists, compare it with the version number of the existing data, if it is equal, write it, and add 1 to the version number; if it is not equal, return an error; if the data does not exist, ignore the incoming version number and write After the import is successful, the data version number becomes 1  
> **ABS**：Absolute version number, regardless of whether the data exists, overwrite the specified version number 
> **FLAGS**：The type is uint32_t to support the memcached protocol. If UINT_MAX is exceeded, an error will be returned. The default value is 0 by default  
> **WITHVERSION**：Modify the return value to version instead of "OK" 
> **MIN**：The minimum value of TairString
> **MAX**：Maximum value of TairString

Return value：
> Type：Double  
> Success：return cur value

Usage example：

```shell
127.0.0.1:6379> EXSET foo 100
OK
127.0.0.1:6379> EXINCRBYFLOAT foo 10.123
"110.123"
127.0.0.1:6379> EXINCRBYFLOAT foo 20 MAX 100
(error) ERR increment or decrement would overflow
127.0.0.1:6379> EXINCRBYFLOAT foo 20 MIN 100
"130.123"
127.0.0.1:6379> EXGET foo
1) "130.123"
2) (integer) 3
127.0.0.1:6379>
```

## EXCAS

Grammar and complexity：
> EXCAS <key> <newvalue> <version>  
> time complexity：O(1)

Command description：
> CAS（Compare And Set

Return value：
> Type：List<String>/List<byte[]>  
> Success：["OK", "", version]
> failed：["Err", value, version]

Usage example：
```shell
127.0.0.1:6379> EXSET foo bar
OK
127.0.0.1:6379> EXCAS foo bzz 1
1) OK
2)
3) (integer) 2
127.0.0.1:6379> EXGET foo
1) "bzz"
2) (integer) 2
127.0.0.1:6379> EXCAS foo bee 1
1) ERR update version is stale  
2) "bzz"
3) (integer) 2
127.0.0.1:6379>
```

## EXCAD

Grammar and complexity：
> EXCAD <key> <version>  
> time complexity：O(1)  

Command description：
> CAD（Compare And Delete）

Return value：
> Type：Long  
> Success：1  
> key not exists：-1  
> failed：0  

Usage example：
```shell
127.0.0.1:6379> EXSET foo bar
OK
127.0.0.1:6379> EXGET foo
1) "bar"
2) (integer) 1
127.0.0.1:6379> EXCAD not-exists 1
(integer) -1
127.0.0.1:6379> EXCAD foo 0
(integer) 0
127.0.0.1:6379> EXCAD foo 1
(integer) 1
127.0.0.1:6379> EXGET foo
(nil)
127.0.0.1:6379>
```

## EXAPPEND

Grammar and complexity：
> EXAPPEND \<key\> \<value\> [NX|XX][ver version | abs version]  
> time complexity：O(1)

Command description：
> Append string to key

Parameter Description：
> **key**: The key used to locate the string      
> **value**: The string that will be appended 
> **NX**：Write when data does not exist  
> **XX**：Write when data exists   
> **VER**：Version number, if the data exists, compare it with the version number of the existing data, if it is equal, write it, and add 1 to the version number; if it is not equal, return an error; if the data does not exist, ignore the incoming version number and write After the import is successful, the data version number becomes 1  
> **ABS**：Absolute version number, regardless of whether the data exists, overwrite the specified version number 
  
Return value：
> Type：Long  
> Success：cur version  

Usage example：
```shell
127.0.0.1:6379> EXSET foo bar
OK
127.0.0.1:6379> EXCAS foo bzz 1
1) OK
2)
3) (integer) 2
127.0.0.1:6379> EXGET foo
1) "bzz"
2) (integer) 2
127.0.0.1:6379> EXCAS foo bee 1
1) ERR update version is stale   
2) "bzz"
3) (integer) 2
127.0.0.1:6379>
```
  
## EXPREPEND

Grammar and complexity：

> EXPREPEND \<key\> \<value\> [NX|XX][ver version | abs version]  
> time complexity：O(1)

Command description：
> Perform string prepend operation on key

Parameter Description：  
> **key**: The key used to locate the string      
> **value**: The value of TairString to be append  
> **NX**：Write when data does not exist  
> **XX**：Write when data exists  

Return value：
> Type：Long  
> Success：cur version   

Usage example：
```shell
127.0.0.1:6379> EXSET foo bar
OK
127.0.0.1:6379> EXCAS foo bzz 1
1) OK
2)
3) (integer) 2
127.0.0.1:6379> EXGET foo
1) "bzz"
2) (integer) 2
127.0.0.1:6379> EXCAS foo bee 1
1) ERR update version is stale  
2) "bzz"
3) (integer) 2
127.0.0.1:6379>
```

## EXGAE

Grammar and complexity：

> EXGAE \<key\> [EX time | EXAT time | PX time | PXAT time]  
> time complexity：O(1)

Command description：

> GAE（Get And Expire）, **This command will not increment version**  

Parameter Description：
> **key**: The key used to locate the string       
> **EX**：Relative expiration time in seconds      
> **EXAT**：Absolute expiration time in seconds    
> **PX**：Relative expiration time in milliseconds    
> **PXAT**：Millisecond absolute expiration time

Return value：
> Type：List\<String\>/List\<byte[]\>  
> Succuss return value+version+flags  
  
Usage example:
```shell
127.0.0.1:6379> EXSET foo bar
OK
127.0.0.1:6379> EXCAS foo bzz 1
1) OK
2)
3) (integer) 2
127.0.0.1:6379> EXGET foo
1) "bzz"
2) (integer) 2
127.0.0.1:6379> EXCAS foo bee 1
1) ERR update version is stale  # Note that what is returned here is a simple string (returning error will cause Jedis to throw an exception)
2) "bzz"
3) (integer) 2
127.0.0.1:6379>
```

<br/>
  
## BUILD

```
mkdir build  
cd build  
cmake ../ && make -j
```
then the tairstring_module.so library file will be generated in the lib directory
## TEST

1. Modify the path in the tairstring.tcl file in the `tests` directory to `set testmodule [file your_path/tairstring_module.so]`
2. Add the path of the tairstring.tcl file in the `tests` directory to the all_tests of redis test_helper.tcl
3. run ./runtest --single tairstring


##
## Client
### Java : https://github.com/aliyun/alibabacloud-tairjedis-sdk

