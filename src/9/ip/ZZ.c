devip.c:828:		cnt = qread(c->rq, a, n);  //%
devip.c:864:		return qbread(c->rq, n);
devip.c:1491:	qreopen(c->rq);

devip.c:1246:		if(c->wq == nil)
devip.c:1249:		qwrite(c->wq, a, n);
devip.c:1360:		if(c->wq == nil)
devip.c:1366:		qbwrite(c->wq, bp);
devip.c:1492:	qreopen(c->wq);



esp.c:185:			qhangup(c->rq, nil);
esp.c:205:	c->rq = qopen(64*1024, Qmsg, 0, 0);
esp.c:214:	qclose(c->rq);
esp.c:415:	if(qfull(c->rq)){
esp.c:421:		qpass(c->rq, bp);
esp.c:459:		qhangup(c->rq, msg);

esp.c:177:			qhangup(c->wq, nil);
esp.c:206:	c->wq = qopen(64*1024, Qkick, espkick, c);
esp.c:215:	qclose(c->wq);
esp.c:241:	bp = qget(c->wq);
esp.c:460:		qhangup(c->wq, msg);



gre.c:99:	c->rq = qopen(64*1024, Qmsg, 0, c);
gre.c:119:	qclose(c->rq);
gre.c:226:	if(qlen(c->rq) > 64*1024)
gre.c:232:		qpass(c->rq, bp);

gre.c:100:	c->wq = qbypass(grekick, c);
gre.c:120:	qclose(c->wq);



icmp.c:117:	c->rq = qopen(64*1024, Qmsg, 0, c);
icmp.c:140:		c->rq ? qlen(c->rq) : 0,
icmp.c:161:	qclose(c->rq);
icmp6.c:185:	c->rq = qopen(64*1024, Qmsg, 0, c);

icmp.c:118:	c->wq = qbypass(icmpkick, c);
icmp.c:141:		c->wq ? qlen(c->wq) : 0
icmp.c:162:	qclose(c->wq);
icmp6.c:186:	c->wq = qbypass(icmpkick6, c);



il.c:268:		c->rq ? qlen(c->rq) : 0,
il.c:322:	qclose(c->rq);
il.c:367:		qhangup(c->rq, nil);
il.c:425:	c->rq = qopen(Maxrq, 0, 0, c);

il.c:269:		c->wq ? qlen(c->wq) : 0,
il.c:323:	qclose(c->wq);
il.c:426:	c->wq = qbypass(ilkick, c);



ipifc.c:184:	qreopen(c->rq);
ipifc.c:346:	c->rq = qopen(QMAX, 0, 0, 0);

ipifc.c:315:	bp = qget(c->wq);
ipifc.c:348:	c->wq = qopen(QMAX, Qkick, ipifckick, c);



ipmux.c:619:	c->rq = qopen(64*1024, Qmsg, 0, c);
ipmux.c:639:	qclose(c->rq);
ipmux.c:770:			if(qpass(c->rq, bp) < 0)

ipmux.c:620:	c->wq = qopen(64*1024, Qkick, ipmuxkick, c);
ipmux.c:640:	qclose(c->wq);
ipmux.c:665:	bp = qget(c->wq);



rudp.c:270:	c->rq = qopen(64*1024, Qmsg, 0, 0);
rudp.c:293:	qclose(c->rq);
rudp.c:611:	if(qfull(c->rq)) {
rudp.c:617:		qpass(c->rq, bp);
rudp.c:960:	if(seq == 0 || qfull(c->rq))

rudp.c:271:	c->wq = qopen(64*1024, Qkick, rudpkick, c);
rudp.c:294:	qclose(c->wq);
rudp.c:358:	bp = qget(c->wq);



tcp.c:473:		c->rq ? qlen(c->rq) : 0,
tcp.c:514:	qhangup(c->rq, nil);
tcp.c:517:	qflush(c->rq);
tcp.c:627:	c->rq = qopen(QMAX, Qcoalesce, tcpacktimer, c);

tcp.c:474:		c->wq ? qlen(c->wq) : 0,
tcp.c:515:	qhangup(c->wq, nil);
tcp.c:628:	c->wq = qopen((3*QMAX)/2, Qkick, tcpkick, c);



udp.c:133:		c->rq ? qlen(c->rq) : 0,
udp.c:157:	c->rq = qopen(128*1024, Qmsg, 0, 0);
udp.c:171:	qclose(c->rq);
udp.c:529:	if(qfull(c->rq)){
udp.c:537:	qpass(c->rq, bp);

udp.c:134:		c->wq ? qlen(c->wq) : 0
udp.c:158:	c->wq = qbypass(udpkick, c);
udp.c:172:	qclose(c->wq);
