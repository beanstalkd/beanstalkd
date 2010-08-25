---
layout: base
title: News
---

## Changes

<div>
  <a id='feed' href='http://feeds.feedburner.com/beanstalkd'><img
    src='/beanstalkd/img/feed-icon.png' alt='Subscribe' /></a>
</div>

{% for post in site.posts %}

### {{ post.date | date_to_long_string }}

[{{ post.title }}](/beanstalkd{{ post.url }})

{% endfor %}

