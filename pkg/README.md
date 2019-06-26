How to make a release:

1. Check out master
2. Tag the commit to release devX.Y: git tag dev1.11
3. Write a change log in file News (don't commit it)
4. Run pkg/dist.sh and follow the intructions.
5. Push tags and beanstalkd.github.io
6. Run pkg/mail.sh
