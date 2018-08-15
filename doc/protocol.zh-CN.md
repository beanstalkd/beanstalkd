## Beanstalkd中文协议

### 总括

`beanstalkd`协议基于ASCII编码运行在tcp上。客户端连接服务器并发送指令和数据，然后等待响应并关闭连接。对于每个连接，服务器按照接收命令的序列依次处理并响应。所有整型值都非负的十进制数，除非有特别声明。

### 名称约定

所有名称必须是ASCII码字符串，即包括：

* **字母** (A-Z and a-z)
* **数字** (0-9)
* **连字符** ("-")
* **加号** ("+")
* **斜线** ("/")
* **分号** (";")
* **点** (".")
* **美元符** ("$")
* **下划线** ("_")
* **括号** ("*(*" and "*)*")

**注意**：名称不能以连字符开始，并且是以空白字符结束，每个名称至少包含一个字符。

### 错误说明

| 返回的错误                               | 描述       |
| --------------------------------------- | -------- |
| `OUT_OF_MEMORY\r\n`                     | 服务器没有足够的内存分配给特定的job，客户端应该稍后重试 |
| `INTERNAL_ERROR\r\n`                    | 服务器内部错误，该错误不应该发生，如果发生了，请报告：http://groups.google.com/group/beanstalk-talk. |
| `BAD_FORMAT\r\n`                         | 格式不正确，客户端发送的指令格式出错，有可能不是以\r\n结尾，或者要求整型值等等 |
| `UNKNOWN_COMMAND\r\n`                   | 未知的命令，客户端发送的指令服务器不理解 |

### job的生命周期
一个工作任务job当client使用put命令时创建。在整个生命周期中job可能有四个工作状态：ready，reserved，delayed，buried。在put之后，一个job的典型状态是ready，在ready队列中，它将等待一个worker取出此job并设置为其为reserved状态。worker占有此job并执行，当job执行完毕，worker可以发送一个delete指令删除此job。

| Status              | Description   |
| --------------------| ------------- |
| `ready`             | 等待被取出并处理 |
| `reserved`          | 如果job被worker取出，将被此worker预订，worker将执行此job |
| `delayed`           | 等待特定时间之后，状态再迁移为ready状态 |
| `buried`            | 等待唤醒，通常在job处理失败时

job典型的生命周期

```
   put            reserve               delete
  -----> [READY] ---------> [RESERVED] --------> *poof*
```

job可能的状态迁移
```
   put with delay               release with delay
  ----------------> [DELAYED] <------------.
                        |                   |
                 kick   | (time passes)     |
                        |                   |
   put                  v     reserve       |       delete
  -----------------> [READY] ---------> [RESERVED] --------> *poof*
                       ^  ^                |  |
                       |   \  release      |  |
                       |    `-------------'   |
                       |                      |
                       | kick                 |
                       |                      |
                       |       bury           |
                    [BURIED] <---------------'
                       |
                       |  delete
                        `--------> *poof*
```

## Tubes
一个服务器有一个或者多个tubes，用来储存统一类型的job。每个tube由一个就绪队列与延迟队列组成。每个job所有的状态迁移在一个tube中完成。consumers消费者可以监控感兴趣的tube，通过发送watch指令。consumers消费者可以取消监控tube，通过发送ignore命令。通过watch list命令返回所有监控的tubes，当客户端预订一个job，此job可能来自任何一个它监控的tube。

当一个客户端连接上服务器时，客户端监控的tube默认为defaut，如果客户端提交job时，没有使用use命令，那么这些job就存于名为default的tube中。

tube按需求创建，无论他们在什么时候被引用到。如果一个tube变为空（即no ready jobs，no delayed jobs，no buried jobs）和没有任何客户端引用，它将会被自动删除。

### 指令说明（Commands）
#### 生产者指令说明（Producer Commands）
#### `put`

插入一个job到队列

```
put <pri> <delay> <ttr> <bytes>\r\n
<data>\r\n
```

* `<pri>` 整型值，为优先级，可以为0-2^32（4,294,967,295），值越小优先级越高，默认为1024。
* `<delay>` 整型值，延迟ready的秒数，在这段时间job为delayed状态。
* `<ttr>` -- time to run --整型值，允许worker执行的最大秒数，如果worker在这段时间不能delete，release，bury job，那么job超时，服务器将release此job，此job的状态迁移为ready。最小为1秒，如果客户端指定为0将会被重置为1。
* `<bytes>` 整型值，job body的长度，不包含\r\n，这个值必须小于max-job-size，默认为2^16。
* `<data>`   job body


响应
```
INSERTED <id>\r\n
```
表示插入job成功，id为新job的任务标识，整型值
```
BURIED <id>\r\n
```
如服务器为了增加队列的优先级而，内存不足时返回，id为新job的任务标识，整型值
```
EXPECTED_CRLF\r\n
```
job body必须以\r\n结尾
```
JOB_TOO_BIG\r\n
```
job body的长度超过max-job-size
```
DRAINING\r\n
```
表示服务器资源耗尽，表示服务器已经进入了“drain mode”，服务器再也不能接受连接，客户端应该使用另一个服务器或者断开稍后重试


#### `use`
说明
producer生产者使用，随后使用put命令，将job放置于对应的tube
格式
```
use <tube>\r\n
tube tube的名称，最大为200字节，不存在时将自动创建
```
响应
```
USING <tube>\r\n tube为正在使用的tube名称
```
消费者指令说明（Worker Commands）

#### `reserve`
说明
取出（预订）job，待处理。它将返回一个新预订的job，如果没有job，beanstalkd将直到有job时才发送响应。一旦job状态迁移为reserved,取出job的client被限制在指定的时间（如果设置了ttr）完成，否则超时，job状态重装迁移为ready。
格式
```
reserve\r\n
```
可选的一个相似的命令
`reserve-with-timeout <seconds>\r\n` 设置取job的超时时间，timeout设置为0时，服务器立即响应或者TIMED_OUT，积极的设置超时，将会限制客户端阻塞在取job的请求的时间。
##### 失败响应
```
DEADLINE_SOON\r\n
```

在一个预定的任务的运行时间内,最后一秒会被服务器保持为一个安全边际，在此期间，客户端将无法等候另外一个任务。
如果客户端在安全隔离期间发出一个预留命令，或者安全隔离期到了,客户端在等候一个预定命令。
```
TIMED_OUT\r\n 超时
```
##### 成功响应
```
RESERVED <id> <bytes>\r\n
<data>\r\n
```
成功取出job，id为job id,整型值，job body的长度，不包含\r\n，data为job body

#### `delete`
说明
从队列中删除一个job
格式
```
delete <id>\r\n
```
id为job id
响应
DELETED\r\n 删除成功
NOT_FOUND\r\n job不存在时，或者job的状态不为ready和buried（这种情况是在job执行超时之前，client发送了delete指令）
#### `release`
说明
release指令将一个reserved的job放回ready queue。它通常在job执行失败时使用。
格式
```
release <id> <pri> <delay>\r\n
```
id 为job id，pri为job的优先级，delay为延迟ready的秒数
响应
RELEASED\r\n 表明成功
BURIED\r\n 如服务器为了增加队列的优先级而，内存不足时返回
NOT_FOUND\r\n 如果job不存在或者client没有预订此job

#### `bury`
说明
将一个job的状态迁移为buried，通过kick命令唤醒
格式
```
bury <id> <pri>\r\n
```
id为job id，pri为优先级
响应
BURIED\r\n 表明成功
NOT_FOUND\r\n 如果job不存在或者client没有预订此job
#### `touch`
说明
允许worker请求更多的时间执行job，这个很有用当job需要很长的时间来执行，worker可用周期的告诉服务器它仍然在执行job（可以被DEADLINE_SOON触发）
格式
```
touch <id>\r\n
```
id为job id
响应
TOUCHED\r\n 表明成功
NOT_FOUND\r\n 如果job不存在或者client没有预订此job

#### `watch`
说明
添加监控的tube到watch list列表，reserve指令将会从监控的tube列表获取job，对于每个连接，监控的列表默认为default
格式
```
watch <tube>\r\n
```
tube 为监控的tube名称，名称最大为200字节，如果tube不存在会自动创建
响应
```
WATCHING <count>\r\n 表明成功
```
count 整型值，已监控的tube数量

#### `ignore`
说明
从已监控的watch list列表中移出特定的tube
格式
```
ignore <tube>\r\n
```
tube 为移出的tube名称，名称最多为200字节，如果tube不存在会自动创建
响应
```
WATCHING <count>\r\n 表明成功
```
count 整型值，已监控的tube数量
NOT_IGNORED\r\n 如果client企图忽略其仅有的tube时的响应
其他指令说明（Other Command）

#### `peek`
说明
让client在系统中检查job，有四种形式的命令，其中第一种形式的指令是针对当前使用的tube
格式
```
peek <id>\r\n  返回id对应的job
peek-ready\r\n 返回下一个ready job
peek-delayed\r\n 返回下一个延迟剩余时间最短的job
peek-buried\r\n 返回下一个在buried列表中的job
```
响应
NOT_FOUND\r\n 如果job不存在，或者没有对应状态的job
```
FOUND <id> <bytes>\r\n <data>\r\n
```
id 为对应的job id
bytes job body的字节数
data 为job body

#### `kick`
说明
此指令应用在当前使用的tube中，它将job的状态迁移为ready或者delayed
格式
```
kick <bound>\r\n
```
bound 整型值，唤醒的job上限

响应
```
KICKED <count>\r\n
```
count 为真实唤醒的job数量
kick-job
说明
kick指令的一个变体，可以使单个job被唤醒，使一个状态为buried或者delayed的job迁移为ready，所有的状态迁移都在相同的tube中完成
格式
```
kick-job <id>\r\n
```
id 为job id
响应
NOT_FOUND\r\n 如果job不存在，或者job是不可唤醒的状态
KICKED\r\n 表明成功

#### `stats-job`
说明
统计job的相关信息
格式
```
stats-job <id>\r\n
```
id 为job id
响应
```
NOT_FOUND\r\n 如果job不存在

OK <bytes>\r\n<data>\r\n
```
bytes 为接下来的data区块的长度
data 为YAML file的统计信息
其中YAML file包括的key有：
- `id` 表示job id
- `tube` 表示tube的名称
- `state` 表示job的当前状态
- `pri` 表示job的优先级
- `age` 表示job创建的时间单位秒
- `delay` 是延迟job放入ready队列的整数秒数
- `ttr` 指允许worker执行job的整数秒数
- `time-left` 表示job的状态迁移为ready的时间，仅在job状态为reserved或者delayed时有意义，当job状态为reserved时表示剩余的超时时间。
- `file` 表示包含此job的binlog序号，如果没有开启它将为0
- `reserves` 表示job被reserved的次数
- `timeouts` 表示job出现超时的次数
- `releases` 表示job被released的次数
- `buries` 表示job被buried的次数
- `kicks` 表示job被kiced的次数

#### `stats-tube`
**说明**
统计tube的相关信息
**格式**
```
stats-tube <tube>\r\n
```
tube 为对应的tube的名称，最多为200字节
**响应**
```
NOT_FOUND\r\n 如果tube不存在
OK <bytes>\r\n<data>\r\n
```
bytes 为接下来的data区块的长度
data 为YAML file的统计信息
其中YAML file包括的key有：
- `name` 表示tube的名称
- `current-jobs-urgent` 此tube中优先级小于1024状态为ready的job数量
- `current-jobs-ready` 此tube中状态为ready的job数量
- `current-jobs-reserved` 此tube中状态为reserved的job数量
- `current-jobs-delayed` 此tube中状态为delayed的job数量
- `current-jobs-bureid` 此tube中状态为buried的job数量
- `total-jobs` 此tube中创建的所有job数量
- `current-using` 使用此tube打开的连接数
- `current-wating` 使用此tube打开连接并且等待响应的连接数
- `current-watching` 打开的连接监控此tube的数量
- `pause` 此tube暂停的秒数
- `cmd-delete` 此tube中总共执行的delete指令的次数
- `cmd-pause-tube` 此tube中总共执行pause-tube指令的次数
- `pause-time-left` 此tube暂停剩余的秒数

#### `stats`
**说明**
返回整个消息队列系统的整体信息
**格式**
```
stats\r\n
```
**响应**
```
OK <bytes>\r\n<data>\r\n
```
bytes 为接下来的data区块的长度
data 为YAML file的统计信息
其中YAML file包括的key有（所有的信息都累积的，自从beanstalkd进程启动以来，这些信息不储存在binlog中）：
- `current-jobs-urgent` 优先级小于1024状态为ready的job数量
- `current-jobs-ready` 状态为ready的job数量
- `current-jobs-reserved` 状态为reserved的job数量
- `current-jobs-delayed` 状态为delayed的job数量
- `current-jobs-bureid` 状态为buried的job数量
- `cmd-put` 总共执行put指令的次数
- `cmd-peek` 总共执行peek指令的次数
- `cmd-peek-ready` 总共执行peek-ready指令的次数
- `cmd-peek-delayed` 总共执行peek-delayed指令的次数
- `cmd-peek-buried` 总共执行peek-buried指令的次数
- `cmd-reserve` 总共执行reserve指令的次数
- `cmd-use` 总共执行use指令的次数
- `cmd-watch` 总共执行watch指令的次数
- `cmd-ignore` 总共执行ignore指令的次数
- `cmd-release` 总共执行release指令的次数
- `cmd-bury` 总共执行bury指令的次数
- `cmd-kick` 总共执行kick指令的次数
- `cmd-stats` 总共执行stats指令的次数
- `cmd-stats-job` 总共执行stats-job指令的次数
- `cmd-stats-tube` 总共执行stats-tube指令的次数
- `cmd-list-tubes` 总共执行list-tubes指令的次数
- `cmd-list-tube-used` 总共执行list-tube-used指令的次数
- `cmd-list-butes-watched` 总共执行list-tubes-watched指令的次数
- `cmd-pause-tube` 总共执行pause-tube指令的次数
- `job-timeouts` 所有超时的job的总共数量
- `total-jobs` 创建的所有job数量
- `max-job-size` job的数据部分最大长度
- `current-tubes` 当前存在的tube数量
- `current-connections` 当前打开的连接数
- `current-producers` 当前所有的打开的连接中至少执行一次put指令的连接数量
- `current-workers` 当前所有的打开的连接中至少执行一次reserve指令的连接数量
- `current-waiting` 当前所有的打开的连接中执行reserve指令但是未响应的连接数量
- `total-connections` 总共处理的连接数
- `pid` 服务器进程的id
- `version` 服务器版本号
- `rusage-utime` 进程总共占用的用户CPU时间
- `rusage-stime` 进程总共占用的系统CPU时间
- `uptime` 服务器进程运行的秒数
- `binlog-oldest-index` 开始储存jobs的binlog索引号
- `binlog-current-index` 当前储存jobs的binlog索引号
- `binlog-max-size binlog`的最大容量
- `binlog-records-written` binlog累积写入的记录数
- `binlog-records-migrated` is the cumulative number of records written as part of compaction.
- `id` 一个随机字符串，在beanstalkd进程启动时产生
- `hostname` 主机名

#### `list-tubes`
说明
列表所有存在的tube
格式
```
list-tubes\r\n
```

响应
```
OK <bytes>\r\n

<data>\r\n
```
bytes 为接下来的data区块的长度
data 为YAML file，包含所有的tube名称

#### `list-tube-used`
说明
列表当前client正在use的tube
格式
```
list-tube-used\r\n
```
响应
```
USING <tube>\r\n
```
tube 为tube名称

#### `list-tubes-watched`
说明
列表当前client watch的tube
格式
```
list-tubes-watched\r\n
```
响应
```
OK <bytes>\r\n

<data>\r\n
```
bytes 为接下来的data区块的长度
data 为YAML file，包含所有的tube名称

#### `quit`
说明
关闭连接
格式
```
quit\r\n
```

#### `pause-tube`
##### 说明
此指令针对特定的tube内所有新的job延迟给定的秒数
##### 格式
```
pause-tube <tube-name> <delay>\r\n
```
##### 响应
```
PAUSED\r\n 表示成功
NOT_FOUND\r\n tube不存在
```


>Translated by PHPBoy :http://www.phpboy.net/ and fzb.me
