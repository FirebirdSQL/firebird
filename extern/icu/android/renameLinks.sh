MAJOR=52
MINOR=1

rm -f libicu*so
for i in libicu*.${MINOR}; do mv $i `basename $i .${MINOR}`; done
for i in libicu*.${MAJOR}; do ln -s $i `basename $i .${MAJOR}`; done
