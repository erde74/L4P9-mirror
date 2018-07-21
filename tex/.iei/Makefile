all:    LPkit.pdf

LPkit.pdf: LPkit.tex
	platex  LPkit
	platex  LPkit
	dvipdfmx   LPkit

clean:
	rm  *.log
	rm  *.dvi
	rm  *.aux
	rm  *~

