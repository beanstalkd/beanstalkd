
push:
	rsync -av --delete --exclude .git --exclude Makefile . xph.us:beanstalkd/.
