all:   LP49manual.pdf  LP49internal.pdf  LP49NW.pdf  LP49kit.pdf

LP49manual.pdf: LP49manual.tex
	platex  LP49manual
	platex  LP49manual
	dvipdfmx  LP49manual

LP49internal.pdf: LP49internal.tex
	platex  LP49internal
	platex  LP49internal
	dvipdfmx  LP49internal


LP49NW.pdf: LP49NW.tex
	platex  LP49NW
	platex  LP49NW
	dvipdfmx  LP49NW


L4aobj.pdf: L4aobj.tex
	platex  L4aobj
	platex  L4aobj
	dvipdfmx  L4aobj

LP49kit.pdf: LP49kit.tex
	platex  LP49kit
	platex  LP49kit
	dvipdfmx  LP49kit



clean:
	rm  *.log
	rm  *.dvi
	rm  *.aux
	rm  *~

