
push:
	rsync -av --exclude .git --exclude Makefile . xph.us:beanstalkd/.
