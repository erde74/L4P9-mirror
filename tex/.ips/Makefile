all:    kitOS.pdf

kitOS.pdf: kitOS.tex
	platex  kitOS
	platex  kitOS
	dvipdfmx  -l kitOS

clean:
	rm  *.log
	rm  *.dvi
	rm  *.aux
	rm  *~

