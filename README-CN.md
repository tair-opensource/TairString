![](https://img.shields.io/badge/license-Apache--2.0-green)
![](https://img.shields.io/badge/PRs-welcome-green)
[![CMake](https://github.com/alibaba/TairString/actions/workflows/cmake.yml/badge.svg)](https://github.com/alibaba/TairString/actions/workflows/cmake.yml)
[![CI](https://github.com/alibaba/TairString/actions/workflows/ci.yml/badge.svg)](https://github.com/alibaba/TairString/actions/workflows/ci.yml)
[![Docker Image CI](https://github.com/alibaba/TairString/actions/workflows/docker-image.yml/badge.svg)](https://github.com/alibaba/TairString/actions/workflows/docker-image.yml)  


<div align=center>
<img src="imgs/tairstring_logo.jpg" width="500"/>
</div>

## 简介  [英文说明](README.md)
TairString 是阿里巴巴 Tair 团队开发并开源的一个 redis module，主要包含两个功能：

- 对 redis 原生 string 进行功能增强，增加了 CAS/CAD 两个命令
- 新增数据类型`exstrtype`， 和原生 string 相比，其 value 上可以指定 version，从而可以方便的实现分布式锁等功能，同时 value 上还可以设置 flags，以支持 memcached 协议

<br/>

## 我们的modules

[TairHash](https://github.com/alibaba/TairHash): 和redis hash类似，但是可以为field设置expire和version，支持高效的主动过期和被动过期   
[TairZset](https://github.com/alibaba/TairZset): 和redis zset类似，但是支持多（最大255）维排序，同时支持incrby语义，非常适合游戏排行榜场景   
[TairString](https://github.com/alibaba/TairString): 和redis string类似，但是支持设置expire和version，并提供CAS/CAD等实用命令，非常适用于分布式锁等场景  



<br/>


## 快速开始

### 增删改查

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

## Docker
```
docker run -p 6379:6379 tairmodule/tairstring:latest
```

# CAS/CAD - Redis 原生 String 的增强

## 命令详解

### CAS

#### 语法及复杂度：

> CAS <Key> <oldvalue> <newvalue> [EX seconds][exat timestamp] [PX milliseconds][pxat timestamp]  
> 时间复杂度：O(1)

#### 命令描述：

> CAS（Compare And Set），当 key 对应的 string 当前值和 oldvalue 相等时才将其值设置为 newvalue

#### 参数描述：

> **key**: 用于定位 string 的键  
> **oldvalue**: 只有 string 当前值和 oldvalue 相等时才允许修改其值  
> **newvalue**：string 当前值和 oldvalue 相等时将值设置为 newvalue  
> **EX**：秒级相对过期时间  
> **EXAT**：秒级绝对过期时间  
> **PX**：毫秒级相对过期时间  
> **PXAT**：毫秒级绝对过期时间

#### 返回值：

> 返回类型：Long  
> 成功：成功更新值返回 1，key 不存在返回-1，更新值失败返回 0  
> 错误（如语法错误）：返回对应异常信息

#### 使用示例：

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

#### 语法及复杂度：

> CAD \<key\> \<value\>  
> 时间复杂度：O(1)

#### 命令描述：

> CAD（Compare And Delete），当 value 和引擎中 value 相等时候删除 Key

#### 参数描述：

> **key**: 用于定位 string 的键  
> **value**: 只有 string 当前值和 value 相等时才允许删除

#### 返回值：

> 返回类型：Long  
> 成功：成功删除 key 返回 1，key 不存在返回-1，删除 key 失败返回 0  
> 错误（如语法错误）：返回对应异常信息

#### 使用示例：

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

# exstrtype - 一种带版本号和兼容 memcached 语义的 String

## 命令介绍

## 命令总览

| 命令          | 语法                                                                                                                                                                             | 含义                                                                                                              |
| ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| EXSET         | EXSET \<key\> \<value\> [EX time][px time] [EXAT time][pxat time] [NX &#124; XX][ver version &#124; abs version] [FLAGS flags][withversion]                                      | 将 value 保存到 key 中，各参数含义见后面具体解释。                                                                |
| EXGET         | EXGET \<key\> [WITHFLAGS]                                                                                                                                                        | 返回 TairStr 的 value + version                                                                                   |
| EXSETVER      | EXSETVER \<key\> \<version\>                                                                                                                                                     | 直接对一个 key 设置 version，类似于 EXSET ABS                                                                     |
| EXINCRBY      | EXINCRBY \<key\> \<num\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx &#124; xx] [VER version &#124; ABS version][min minval] [MAX maxval][nonegative] [WITHVERSION] | 对 Key 做自增自减操作，num 的范围为 long。                                                                        |
| EXINCRBYFLOAT | EXINCRBYFLOAT \<key\> \<num\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx &#124; xx] [VER version &#124; ABS version][min minval] [MAX maxval]                      | 对 Key 做自增自减操作，num 的范围为 double。                                                                      |
| EXCAS         | EXCAS \<key\> \<newvalue\> \<version\>                                                                                                                                           | 指定 version 将 value 更新，当引擎中的 version 和指定的相同时才更新成功，不成功会返回旧的 value 和 version。      |
| EXCAD         | EXCAD \<key\> \<version\>                                                                                                                                                        | 当指定 version 和引擎中 version 相等时候删除 Key，否则失败。                                                      |
| EXAPPEND      | EXAPPEND \<key\> \<value\> [NX\|XX][ver version \| abs version]                                                                                                                  | 对 key 做字符串 append 操作                                                                                       |
| EXPREPEND     | EXPREPEND \<key\> \<value\> [NX\|XX][ver version \| abs version]                                                                                                                 | 对 key 做字符串 prepend 操作                                                                                      |
| EXGAE         | EXGAE \<key\> [EX time][px time] [EXAT time][pxat time]                                                                                                                          | GAE（Get And Expire），返回 TairString 的 value+version+flags，同时设置 key 的 expire. **该命令不会自增 version** |
|               |                                                                                                                                                                                  |                                                                                                                   |

<br/>

## EXSET

语法及复杂度：

> EXSET \<key\> \<value\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx | xx] [VER version | ABS version][flags flags] [WITHVERSION]  
> 时间复杂度：O(1)

命令描述：  
> 将 value 保存到 key 中

参数描述：  
> **key**: 用于定位 TairString 的键  
> **value**: 要设置的 TairString 的值  
> **EX**：秒级相对过期时间  
> **EXAT**：秒级绝对过期时间  
> **PX**：毫秒级相对过期时间  
> **PXAT**：毫秒级绝对过期时间  
> **NX**：当数据不存在时写入    
> **XX**：当数据存在时写入  
> **VER**：版本号，如果数据存在，和已经存在的数据的版本号做比较，如果相等，写入，并版本号加 1；如果不相等，返回出错；如果数据不存在，忽略传入的版本号，写入成功之后，数据版本号变为 1  
> **ABS**：绝对版本号，不论数据是否存在，覆盖为指定的版本号    
> **FLAGS**：类型为uint32_t，以支持 memcached 协议，超出 UINT_MAX 返回出错，缺省时默认值为 0    
> **WITHVERSION**：修改返回值为 version 而不是"OK" 
 
返回值  ：  
> 返回类型：String    
> 成功：OK    
> 其他错误返回异常    

使用示例：
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

语法及复杂度：

> EXGET \<key\> [WITHFLAGS]  
> 时间复杂度：O(1)  

命令描述：
> 返回 TairStr 的 value + version  

参数描述：  
> **key**: 用于定位 TairString 的键  
> **WITHFLAGS**: 设置该参数则会多返回一个 flags  

返回值：

> 返回类型：List<String>/List<byte[]>  
> 成功：value+version  
> 其他错误返回异常  

使用示例：
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

语法及复杂度：

> EXSETVER \<key\> \<version\>  
> 时间复杂度：O(1)  

命令描述：
> 直接对一个 key 设置 version，类似于 EXSET ABS  
 
参数描述：  
> **key**: 用于定位 TairString 的键  
> **version**: 强制设置的 TairString 的版本号  

返回值：
> 返回类型：Long  
> 成功：1  
> key 不存在：0  
> 其他错误返回异常  

使用示例：

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

语法及复杂度：
> EXINCRBY | EXINCRBY \<key\> \<num\> [EX time][px time] [EXAT time][exat time] [PXAT time][nx | xx] [VER version | ABS version][min minval] [MAX maxval][nonegative] [WITHVERSION]
> 时间复杂度：O(1)

命令描述：  
> 对 Key 做自增自减操作，num 的范围为 long  

参数描述：
> **key**: 定位 TairString 的键  
> **num**: TairString 自增的数值，必须为自然数  
> **EX**：秒级相对过期时间  
> **EXAT**：秒级绝对过期时间  
> **PX**：毫秒级相对过期时间  
> **PXAT**：毫秒级绝对过期时间    
> **NX**：当数据不存在时写入  
> **XX**：当数据存在时写入    
> **VER**：版本号，如果数据存在，和已经存在的数据的版本号做比较，如果相等，写入，并版本号加 1；如果不相等，返回出错；如果数据不存在，忽略传入的版本号，写入成功之后，数据版本号变为 1  
> **ABS**：绝对版本号，不论数据是否存在，覆盖为指定的版本号  
> **MIN**：TairString 值的最小值  
> **MAX**：TairString 值的最大值  
> **NONEGATIVE**：设置后，若 incrby 的结果小于 0 则将 value 置为 0  
> **WITHVERSION**：额外返回一个 version  

返回值：
> 返回类型：Long  
> 成功：引擎的 value 值  
> 其他错误返回异常  

使用示例：

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

语法及复杂度：

> EXINCRBYFLOAT <key> <num> [EX time][px time] [EXAT time][exat time] [PXAT time][nx | xx] [VER version | ABS version][min minval] [MAX maxval]  
> 时间复杂度：O(1)

命令描述：
> 对 Key 做自增自减操作，num 的范围为 double

参数描述：  
> **key**: 定位 TairString 的键  
> **num**: TairString 自增的数值，浮点数类型  
> **EX**：秒级相对过期时间  
> **EXAT**：秒级绝对过期时间  
> **PX**：毫秒级相对过期时间  
> **PXAT**：毫秒级绝对过期时间  
> **NX**：当数据不存在时写入  
> **XX**：当数据存在时写入  
> **VER**：版本号，如果数据存在，和已经存在的数据的版本号做比较，如果相等，写入，并版本号加 1；如果不相等，返回出错；如果数据不存在，忽略传入的版本号，写入成功之后，数据版本号变为 1  
> **ABS**：绝对版本号，不论数据是否存在，覆盖为指定的版本号  
> **MIN**：TairString 值的最小值  
> **MAX**：TairString 值的最大值  

返回值：
> 返回类型：Double  
> 成功：引擎的 value 值  
> 其他错误返回异常  

使用示例：

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

语法及复杂度：
> EXCAS <key> <newvalue> <version>  
> 时间复杂度：O(1)

命令描述：
> CAS（Compare And Set），对比指定 version 将 value 更新，当引擎中的 version 和指定的相同时才更新成功，不成功会返回旧的 value 和 version  

返回值：
> 返回类型：List<String>/List<byte[]>  
> 成功：["OK", "", version]，中间值""是空串无意义，version 是当前的 version  
> 删除失败：["Err", value, version]。错误是"ERR update version is stale", value 和 version 都是引擎最新的  
> 其他错误返回异常  

使用示例：
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
1) ERR update version is stale  # 注意这里返回的是简单字符串（返回错误类型会导致 Jedis 抛异常）
2) "bzz"
3) (integer) 2
127.0.0.1:6379>
```

## EXCAD

语法及复杂度：
> EXCAD <key> <version>  
> 时间复杂度：O(1)  

命令描述：
> CAD（Compare And Delete），当指定 version 和引擎中 version 相等时候删除 Key，否则失败  

返回值：
> 返回类型：Long  
> 成功：1  
> key 不存在：-1  
> 删除失败：0  
> 其他错误返回异常  

使用示例：
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

语法及复杂度：
> EXAPPEND \<key\> \<value\> [NX|XX][ver version | abs version]  
> 时间复杂度：O(1)

命令描述：
> 对 key 做字符串 append 操作  

参数描述：
> **key**：定位 TairString 的键  
> **value**：需要 append 的字符串  
> **NX**：当数据不存在时写入  
> **XX**：当数据存在时写入  
> **VER**：版本号，如果数据存在，和已经存在的数据的版本号做比较，如果相等，写入，并版本号加 1；如果不相等，返回出错；如果数据不存在，忽略传入的版本号，写入成功之后，数据版本号变为 1  
> **ABS**：绝对版本号，不论数据是否存在，覆盖为指定的版本号  
  
返回值：
> 返回类型：Long  
> 成功：自增后的 version  
> 其他错误返回异常  

使用示例：
```shell
127.0.0.1:6379> exappend exstringkey foo
(integer) 1
127.0.0.1:6379> exget exstringkey
1) "foo"
2) (integer) 1
127.0.0.1:6379> exappend exstringkey bar
(integer) 2
127.0.0.1:6379> exget exstringkey
1) "foobar"
2) (integer) 2
```
  
## EXPREPEND

语法及复杂度：

> EXPREPEND \<key\> \<value\> [NX|XX][ver version | abs version]  
> 时间复杂度：O(1)

命令描述：
> 对 key 做字符串 prepend 操作  

参数描述：  
> **key**：定位 TairString 的键  
> **value**：需要 prepend 的字符串  
> **NX**：当数据不存在时写入  
> **XX**：当数据存在时写入  
> **VER**：版本号，如果数据存在，和已经存在的数据的版本号做比较，如果相等，写入，并版本号加 1；如果不相等，返回出错；如果数据不存在，忽略传入的版本号，写入成功之后，数据版本号变为 1  
> **ABS**：绝对版本号，不论数据是否存在，覆盖为指定的版本号

返回值：
> 返回类型：Long  
> 成功：自增后的 version  
> 其他错误返回异常  

使用示例：
```shell
127.0.0.1:6379> exprepend exstringkey foo
(integer) 1
127.0.0.1:6379> exget exstringkey
1) "foo"
2) (integer) 1
127.0.0.1:6379> exprepend exstringkey bar
(integer) 2
127.0.0.1:6379> exget exstringkey
1) "barfoo"
2) (integer) 2
```

## EXGAE

语法及复杂度：

> EXGAE \<key\> [EX time | EXAT time | PX time | PXAT time]  
> 时间复杂度：O(1)

命令描述：

> GAE（Get And Expire），返回 TairString 的 value+version+flags，同时设置 key 的 expire. **该命令不会自增 version**  

参数描述：
> **key**：定位 TairString 的键    
> **EX**：秒级相对过期时间     
> **EXAT**：秒级绝对过期时间    
> **PX**：毫秒级相对过期时间    
> **PXAT**：毫秒级绝对过期时间    

返回值：
> 返回类型：List\<String\>/List\<byte[]\>  
> 成功：value+version+flags  
> 其他错误返回异常  
  
使用示例：
```shell
127.0.0.1:6379> exset exstringkey foo ex 10 flags 123 WITHVERSION
(integer) 1
127.0.0.1:6379> ttl exstringkey
(integer) 9
127.0.0.1:6379> exgae exstringkey ex 20
1) "foo"
2) (integer) 1
3) (integer) 123
127.0.0.1:6379> ttl exstringkey
(integer) 18
127.0.0.1:6379> debug sleep 20
OK
(20.00s)
127.0.0.1:6379> ttl exstringkey
(integer) -2
127.0.0.1:6379>
```

<br/>
  
## 编译及使用

```
mkdir build  
cd build  
cmake ../ && make -j
```
编译成功后会在lib目录下产生tairstring_module.so库文件

```
./redis-server --loadmodule /path/to/tairstring_module.so
```
## 测试方法

1. 修改`tests`目录下tairstring.tcl文件中的路径为`set testmodule [file your_path/tairstring_module.so]`
2. 将`tests`目录下tairstring.tcl文件路径加入到redis的test_helper.tcl的all_tests中
3. 在redis根目录下运行./runtest --single tairstring


##
## 客户端
### Java : https://github.com/aliyun/alibabacloud-tairjedis-sdk
### 其他语言：可以参考 java 的 sendcommand 自己封装
