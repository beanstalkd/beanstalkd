

all: clean_site

clean_site: clean _site

_site:
	jekyll --pygments
	ln -s . _site/beanstalkd

server: clean_site
	jekyll --pygments --auto --server 7000

clean:
	rm -rf _site

.PHONY: server clean
