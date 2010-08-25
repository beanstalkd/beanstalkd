---
layout: base
title: About
---

Beanstalk is a simple, fast work queue.

Its interface is generic, but was originally designed for reducing the latency
of page views in high-volume web applications by running time-consuming tasks
asynchronously.

## News

<a id='feed' href='http://feeds.feedburner.com/beanstalkd'><img
  src='/beanstalkd/img/feed-icon.png' alt='Subscribe' /></a>

{% for post in site.posts limit:1 %}

### {{ post.date | date_to_long_string }}

[{{ post.title }}](/beanstalkd{{ post.url }})

{% endfor %}

[More news...](news.html)

## Run It

First, run `beanstalkd` on one or more machines. There is no configuration
file and only a handful of command-line options.

{% highlight bash %}
$ ./beanstalkd -d -l 10.0.1.5 -p 11300
{% endhighlight %}

This starts up `beanstalkd` as a daemon listening on address
10.0.1.5, port 11300.

## Use It

Here's an example in Ruby -- see the [client libraries][] to find your
favorite language.

First, have one process put a job into the queue:

{% highlight ruby %}
beanstalk = Beanstalk::Pool.new(['10.0.1.5:11300'])
beanstalk.put('hello')
{% endhighlight %}

Then start another process to take jobs out of the queue and run them:

{% highlight ruby %}
beanstalk = Beanstalk::Pool.new(['10.0.1.5:11300'])
loop do
  job = beanstalk.reserve
  puts job.body # prints "hello"
  job.delete
end
{% endhighlight %}

## History

Philotic, Inc. developed beanstalk to improve the response time for the
[Causes on Facebook][cof] application (with over 9.5 million users). Beanstalk
decreased the average response time for the most common pages to a tiny
fraction of the original, significantly improving the user experience.

## Bugs

Please report any bugs to [the mailing list][mailinglist].

## Thanks

Many thanks to [memcached][memcached] for providing inspiration for simple
protocol design and for the structure of the documentation. Not to mention a
fantastic piece of software!

[cof]: http://apps.facebook.com/causes/
[mailinglist]: http://groups.google.com/group/beanstalk-talk
[memcached]: http://www.danga.com/memcached/
[client_libraries]: http://wiki.github.com/kr/beanstalkd/client-libraries
