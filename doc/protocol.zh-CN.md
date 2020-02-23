# Beanstalkd中文协议

### 总括

`beanstalkd` 协议基于 ASCII 编码运行在 tcp 上. 客户端连接服务器并发送指令和数据，然后等待响应并关闭连接. 对于每个连接，服务器按照接收命令的序列依次处理并响应. 所有整型值都非负的十进制数，除非有特别声明.

### 名称约定

所有名称必须是 ASCII 码字符串，即包括：

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

**注意**：名称不能以连字符开始，并且是以空白字符结束，每个名称至少包含一个字符.

### 错误说明

| 返回的错误                               | 描述       |
| --------------------------------------- | -------- |
| `OUT_OF_MEMORY\r\n`                     | 服务器没有足够的内存分配给特定的 job，客户端应该稍后重试 |
| `INTERNAL_ERROR\r\n`                    | 服务器内部错误，该错误不应该发生，如果发生了，请报告: [http://groups.google.com/group/beanstalk-talk](http://groups.google.com/group/beanstalk-talk).|
| `BAD_FORMAT\r\n`                         | 格式不正确，客户端发送的指令格式出错，有可能不是以 `\r\n` 结尾，或者要求整型值等等 |
| `UNKNOWN_COMMAND\r\n`                   | 未知的命令，客户端发送的指令服务器不理解 |

### job 的生命周期

Client 使用 put 命令创建一个工作任务 job. 在整个生命周期中 job 可能有四个工作状态：ready、reserved、delayed、buried. 在 put 操作之后，一个 job 的典型状态是 ready，在 ready 队列中，它将等待一个 worker 取出此 job 并设置为其为 reserved 状态. worker占有此 job 并执行，当 job 执行完毕，worker 可以发送一个 delete 指令删除此 job.

| Status              | Description   |
| --------------------| ------------- |
| `ready`             | 等待被取出并处理 |
| `reserved`          | 如果 job 被 worker 取出，将被此 worker 预订，worker 将执行此 job |
| `delayed`           | 等待特定时间之后，状态再迁移为 `ready` 状态 |
| `buried`            | 等待唤醒，通常在 job 处理失败时进入该状态

#### job 典型的生命周期

```text
   put            reserve               delete
  -----> [READY] ---------> [RESERVED] --------> *poof*
```

#### job 可能的状态迁移

```text
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

一个 beanstalkd 实例服务可能有一个或者多个 tube，用来储存统一类型的 job.每个 tube 由一个就绪 (`ready`) 队列与延迟 (`delayed`) 队列组成.每个 job 所有的状态迁移在一个 tube 中完成.通过发送 `watch` 指令, 消费者 consumers 可以监控感兴趣的 tube.通过发送 `ignore` 指令, 消费者 consumers 可以取消监控 tube.通过 `list-tubes-watched`命令返回所有监控的 tubes，当客户端预订 (`reserved`) 一个 job，此 job 可能来自任何一个它监控的 tube.

当一个客户端连接上服务器时，客户端监控的 tube 默认为 `defaut`，如果客户端提交 job 时，没有使用 `use` 命令，那么这些 job 就存于名为`default`的 tube 中.

tube 按需求创建，无论他们在什么时候被引用到.如果一个 tube 变为空（即 no ready jobs，no delayed jobs，no buried jobs）和没有任何客户端引用(being watched)，它将会被**自动删除**.

### 指令说明（Commands）

#### 生产者指令说明（Producer Commands）

#### `put`

插入一个 job 到队列

**指令格式**

```text
put <pri> <delay> <ttr> <bytes>\r\n
<data>\r\n
```

* `<pri>` 整型值, 为优先级, 可以为0-2^32 (4,294,967,295) 值越小优先级越高, 默认为1024.
* `<delay>` 整型值，延迟`ready`的秒数，在这段时间 job 为 `delayed` 状态. 最大 delay 值为 2^32-1.
* `<ttr>` -- time to run --整型值，允许 worker 执行的最大秒数，如果 worker 在这段时间不能 delete，release，bury job，那么当 job 超时，服务器将**自动** release 此job，此 job 的状态迁移为`ready`. 最小为 1 秒，如果客户端指定为 0 将会被重置为 1. 最大 ttr 值为 2^32-1.
* `<bytes>` 整型值，job body的长度，不包含`\r\n`，这个值必须小于 `max-job-size`，默认为 2^16.
* `<data>`   job body

**响应**

```text
INSERTED <id>\r\n
```

表示插入 job 成功，id 为新 job 的任务标识，整型值 (uint64)

```text
BURIED <id>\r\n
```

如服务器为了增加队列的优先级而，内存不足时返回，id 为新 job 的任务标识，整型值 (uint64)

```text
EXPECTED_CRLF\r\n
```

job body 必须以 `\r\n` 结尾

```text
JOB_TOO_BIG\r\n
```

job body 的长度超过 `max-job-size`

```text
DRAINING\r\n
```

表示服务器资源耗尽，表示服务器已经进入了`drain mode`，服务器再也不能接受连接，客户端应该使用其他服务器或者断开稍后重试

#### `use`

producer 生产者使用，之后使用的 put 指令，都将会把 job 放置于 use 的 tube 中，如果没有指定 use 的 tube， 任务 job 将会进入默认名称为 `default` 的 tube 

**指令格式**

```text
use <tube>\r\n
tube tube 的名称，最大为 200 字节，不存在时将自动创建
```

**响应**

```text
USING <tube>\r\n tube 为正在使用的tube名称
```

#### 消费者指令说明（Worker Commands）

#### `reserve`

预订(reserved) job 等待处理. beanstalkd 将返回一个新预订的 job，如果没有 job，beanstalkd 将一直等待到有 job 时才发送响应. 一旦 job 状态迁移为 `reserved`, 取出 job 的 client 被限制在指定的时间（如果设置了ttr）完成，否则将超时，job 状态重装迁移为ready.

**指令格式**

```text
reserve\r\n
```

可选的一个相似的命令
`reserve-with-timeout \r\n` 设置取 job 的超时时间，timeout 设置为 0 时，服务器立即响应或者 TIMED_OUT，积极的设置超时，将会限制客户端阻塞在取 job 的请求的时间.

##### 失败响应

```text
DEADLINE_SOON\r\n
```

* 在一个预定的任务 job 的运行时间内, **最后一秒**会被服务器保持为一个**安全边际**，在这个**时间间隔** (1s) 中，client 将无法获取其他任务. 如果客户端在安全隔离期间发出一个预留 (reserve) 指令，或者客户端在等候一个预定 (reserve) 指令返回结果时，client 安全隔离期到达时，将会收到 `DEADLINE_SOON` 回复
* `DEADLINE_SOON` 的返回结果提示 client 这是一个 delete 或者 touch 它所预订(reserved) 的任务 job 的时机，之后 beanstalkd 服务端将会自动释放 `ttr` 到期的 job

```text
TIMED_OUT\r\n 超时
```

##### 成功响应

```text
RESERVED <id> <bytes>\r\n
<data>\r\n
```

成功取出 job:

* `<id>` 为 job id,整型值
* `<bytes>` 为 job body 的长度，不包含`\r\n`，
* `<data>` 为job body

#### `delete`

从队列中删除一个job

**指令格式**

```text
delete <id>\r\n
```

* `<id>` 为 job id

**响应**

```text
DELETED\r\n
```

* 删除成功

```text
NOT_FOUND\r\n
```

* job 不存在时，或者 job 并不为当前的 client 所 reserved;
* job 的状态不为 `ready`和 `buried`（这种情况是在 job 被其他 client 所预订(reserved) 且还未执行超时，此时当前 client 发送了 delete 指令就会收到 `NOT_FOUND` 回复）

#### `release`

release 指令将一个`reserved`的 job 恢复为`ready`. 它通常在 job 执行失败时使用.

**指令格式**

```text
release <id> <pri> <delay>\r\n
```

* `<id>` 为job id
* `<pri>` 为 job 的优先级
* `<delay>` 为延迟`ready`的秒数


**响应**

```text
RELEASED\r\n 表明成功
BURIED\r\n 如服务器为了增加队列的优先级而，内存不足时返回
bury <id> <pri>\r\n
```

* `<id>` 为 job id
* `<pri>` 为优先级

**响应**

```text
BURIED\r\n 表明成功
NOT_FOUND\r\n 如果 job 不存在或者 client 没有预订此 job
```

#### `touch`

允许 worker 请求更多的时间执行 job；当 job 需要更长的时间来执行，这个指令就将会起作用，worker 可用周期性的告诉服务器它仍然在执行job（可以被 `DEADLINE_SOON` 触发）

**指令格式**

```text
touch <id>\r\n
```

* `<id>` 为 job id

**响应**

```text
TOUCHED\r\n 表明成功
NOT_FOUND\r\n 如果 job 不存在或者 client 没有预订此 job
```

#### `watch`

添加监控的 tube 到 watch list 列表，reserve 指令将会从监控的 tube 列表获取 job，对于每个连接，监控的列表默认为 `default`

**指令格式**

```text
watch <tube>\r\n
```

* `<tube>` 为监控的 tube 名称，名称最大为 200 字节，如果 tube 不存在会**自动创建**

**响应**

```text
WATCHING <count>\r\n 表明成功
```

* `<count>` 整型值，已监控的 tube 数量

#### `ignore`

从已监控的 watch list 列表中移出特定的 tube 

**指令格式**

```text
ignore <tube>\r\n
```

* `<tube>` 为移出的 tube 名称，名称最多为 200 字节，如果 tube 不存在会自动创建

**响应**

```text
WATCHING <count>\r\n 表明成功
```

* `<count>` 整型值，已监控的tube数量

```text
NOT_IGNORED\r\n
```

* 如果 client 尝试忽略其仅有的tube时的响应

#### 其他指令说明（Other Command）

#### `peek`

让 client 在系统中检查 job，有四种形式的命令，其中第一种形式的指令是针对当前使用 (use) 的 tube

**指令格式**

```text
peek <id>\r\n  返回 id 对应的 job
peek-ready\r\n 返回下一个 ready job
peek-delayed\r\n 返回下一个延迟剩余时间最短的 job
peek-buried\r\n 返回下一个在 buried 列表中的 job
```

**响应**

```text
NOT_FOUND\r\n
``` 

* 如果 job 不存在，或者没有对应状态的 job

```text
FOUND <id> <bytes>\r\n <data>\r\n
```

* `<id>` 为对应的 job id
* `<bytes>` job body 的字节数
* `<data>` 为 job body

#### `kick`

此指令应用在当前使用 (use) 的 tube 中，它将 job 的状态迁移为`ready`或者`delayed`
**指令格式**

```text
kick <bound>\r\n
```

* `<bound>` 整型值，唤醒的 job 上限

**响应**

```
KICKED <count>\r\n
```
* `<count>` 为真实唤醒的job数量

#### kick-job

kick 指令的一个变体，可以使单个 job 被唤醒，使一个状态为`buried`或者`delayed`的 job迁移为`ready`，所有的状态迁移都在相同的 tube 中完成

**指令格式**

```text
kick-job <id>\r\n
```
* `<id>` 为job id

**响应**
`NOT_FOUND\r\n` 如果 job 不存在，或者 job 是不可唤醒的状态
`KICKED\r\n` 表明成功

#### `stats-job`

统计 job 的相关信息

**指令格式**

```text
stats-job <id>\r\n
```

* `<id>` 为 job id

**响应**

```text
NOT_FOUND\r\n 如果job不存在

OK <bytes>\r\n<data>\r\n
```

* `<bytes>` 为接下来的 data 区块的长度
* `<data>` 为 YAML file 的统计信息

其中 YAML file 包括的 key 有:

* `id` 表示 job id
* `tube` 表示 tube 的名称
* `state` 表示 job 的当前状态
* `pri` 表示 job 的优先级
* `age` 表示 job 创建的时间单位秒
* `time-left` 表示 job 的状态迁移为 ready 的时间，仅在 job 状态为`reserved`或者`delayed`时有意义，当 job 状态为`reserved`时表示剩余的超时时间.
* `file` 表示包含此 job 的`binlog`序号，如果没有开启它将为 0
* `reserves` 表示 job 被`reserved`的次数
* `timeouts` 表示 job 处理的超时时间
* `releases` 表示 job 被`released`的次数
* `buries` 表示 job 被`buried`的次数
* `kicks` 表示 job 被`kiced`的次数

#### `stats-tube`

统计 tube 的相关信息

**指令格式**

```text
stats-tube <tube>\r\n
```

* `<tube>` 为对应的 tube 的名称，最多为 200 字节

**响应**

```text
NOT_FOUND\r\n
```
* 如果tube不存在

```text
OK <bytes>\r\n
<data>\r\n
```

* `<bytes>` 为接下来的 data 区块的长度
* `<data>` 为 YAML file的统计信息

其中 YAML file 包括的 key 有：

* `name` 表示tube的名称
* `current-jobs-urgent` 此 tube 中优先级小于 1024 状态为`ready`的 job 数量
* `current-jobs-ready` 此 tube 中状态为`ready`的 job 数量
* `current-jobs-reserved` 此 tube 中状态为`reserved`的 job 数量
* `current-jobs-delayed` 此 tube 中状态为`delayed`的 job 数量
* `current-jobs-bureid` 此 tube 中状态为`buried`的job数量
* `total-jobs` 此 tube 中创建的所有job数量
* `current-using` 使用此 tube 打开的连接数
* `current-wating` 使用此 tube 打开连接并且等待响应的连接数
* `current-watching` 打开的连接监控此 tube 的数量
* `pause` 此 tube 暂停的秒数
* `cmd-delete` 此 tube 中总共执行的`delete`指令的次数
* `cmd-pause-tube` 此 tube 中总共执行`pause-tube`指令的次数
* `pause-time-left` 此 tube 暂停剩余的秒数

#### `stats`

返回整个消息队列系统的整体信息

**指令格式**

```text
stats\r\n
```

**响应**

```text
OK <bytes>\r\n
<data>\r\n
```
* `<bytes>` 为接下来的 data 区块的长度
* `<data>` 为 YAML file 的统计信息

从 beanstalkd 进程启动以来，所有的信息都累积的，这些信息不储存在 binlog 中
其中 YAML file 包括的key有：

* `current-jobs-urgent` 优先级小于 1024 状态为`ready`的 job 数量
* `current-jobs-ready` 状态为`ready`的 job 数量
* `current-jobs-reserved` 状态为`reserved`的 job 数量
* `current-jobs-delayed` 状态为`delayed`的 job 数量
* `current-jobs-bureid` 状态为`buried`的 job 数量
* `cmd-put` 总共执行`put`指令的次数
* `cmd-peek` 总共执行`peek`指令的次数
* `cmd-peek-ready` 总共执行`peek-ready`指令的次数
* `cmd-peek-delayed` 总共执行`peek-delayed`指令的次数
* `cmd-peek-buried` 总共执行`peek-buried`指令的次数
* `cmd-reserve` 总共执行`reserve`指令的次数
* `cmd-use` 总共执行`use`指令的次数
* `cmd-watch` 总共执行`watch`指令的次数
* `cmd-ignore` 总共执行`ignore`指令的次数
* `cmd-release` 总共执行`release`指令的次数
* `cmd-bury` 总共执行`bury`指令的次数
* `cmd-kick` 总共执行`kick`指令的次数
* `cmd-stats` 总共执行`stats`指令的次数
* `cmd-stats-job` 总共执行`stats-job`指令的次数
* `cmd-stats-tube` 总共执行`stats-tube`指令的次数
* `cmd-list-tubes` 总共执行`list-tubes`指令的次数
* `cmd-list-tube-used` 总共执行`list-tube-used`指令的次数
* `cmd-list-tubes-watched` 总共执行`list-tubes-watched`指令的次数
* `cmd-pause-tube` 总共执行`pause-tube`指令的次数
* `job-timeouts` 所有超时的 job 的总共数量
* `total-jobs` 创建的所有 job 数量
* `max-job-size` job 的数据部分最大长度
* `current-tubes` 当前存在的 tube 数量
* `current-connections` 当前打开的连接数
* `current-producers` 当前所有的打开的连接中至少执行一次 put 指令的连接数量
* `current-workers` 当前所有的打开的连接中至少执行一次 reserve 指令的连接数量
* `current-waiting` 当前所有的打开的连接中执行 reserve 指令但是未响应的连接数量
* `total-connections` 总共处理的连接数
* `pid` 服务器进程的 id
* `version` 服务器版本号
* `rusage-utime` 进程总共占用的用户 CPU 时间
* `rusage-stime` 进程总共占用的系统 CPU 时间
* `uptime` 服务器进程运行的秒数
* `binlog-oldest-index` 开始储存 jobs 的 binlog 索引号
* `binlog-current-index` 当前储存 jobs 的 binlog 索引号
* `binlog-max-size` binlog 的最大容量
* `binlog-records-written` binlog 累积写入的记录数
* `binlog-records-migrated` is the cumulative number of records written as part of compaction.
* `id` 一个随机字符串，在 beanstalkd 进程启动时产生
* `hostname` 主机名

#### `list-tubes`

列出当前 beanstalkd 所有存在的 tubes

**指令格式**

```text
list-tubes\r\n
```

**响应**

```text
OK <bytes>\r\n
<data>\r\n
```

* `<bytes>` 为接下来的 data 区块的长度
* `<data>` 为 YAML file，包含所有的 tube 名称

#### `list-tube-used`

列出当前 client 正在 use 的 tube

**指令格式**

```text
list-tube-used\r\n
```

**响应**

```text
USING <tube>\r\n
```

* `<tube>` 为 tube 名称

#### `list-tubes-watched`

列出当前 client 所 watch 的 tubes

**指令格式**

```text
list-tubes-watched\r\n
```

**响应**

```text
OK <bytes>\r\n
<data>\r\n
```

* `<bytes>` 为接下来的 data 区块的长度
* `<data>` 为 YAML file，包含所有的 tube 名称

#### `quit`

client 向 beanstalkd 发送 `quit` 报文，并关闭连接，beanstalkd 收到该报文后主动关闭连接

**指令格式**

```text
quit\r\n
```

无响应

#### `pause-tube`

此指令针对特定的 tube 内所有新的 job 延迟指定的秒数

**指令格式**

```text
pause-tube <tube-name> <delay>\r\n
```

* `<delay>` 延迟的时间

**响应**

```text
PAUSED\r\n 表示成功
NOT_FOUND\r\n tube 不存在
```


> Translated by PHPBoy :http://www.phpboy.net/ and fzb.me
Revised by Pseudocodes: https://github.com/pseudocodes
