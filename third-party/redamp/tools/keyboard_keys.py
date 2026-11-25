"""Controller navigation for the shared RedAmp text keyboard in UI tests."""
from collections import deque
UP,DOWN,LEFT,RIGHT,A,L,R=103,108,105,106,57,18,20
PAGES=(('1234567890','qwertyuiop','asdfghjkl','zxcvbnm'),
       ('àáâãäåæçčć','èéêëěìíîïł','ñńòóôõöøœř','śšßùúûüýÿž'),
       ('!@#$%^&*()','-_+=[]{}<>','.,:;!?/\\|~',"'\"`€£¥°©®…"))
SHIFT=(('!@#$%^&*()','QWERTYUIOP','ASDFGHJKL','ZXCVBNM'),
       ('ÀÁÂÃÄÅÆÇČĆ','ÈÉÊËĚÌÍÎÏŁ','ÑŃÒÓÔÕÖØŒŘ','ŚŠẞÙÚÛÜÝŸŽ'),PAGES[2])
class Keyboard:
    def __init__(self,keys):self.keys=keys;self.row=1;self.col=0;self.page=0;self.shift=False
    def rows(self):return (*((SHIFT if self.shift else PAGES)[self.page]),('SHIFT',' ','Backspace'))
    def center(self,r,c):
        if r==4:return (120,310,510)[c]
        n=len(self.rows()[r]);pitch=500//n if n>10 else 50
        return (640-n*pitch)//2+c*pitch+3+(pitch-6)//2
    def move(self,r,c,k):
        rows=self.rows()
        if k==LEFT:return r,(c-1)%len(rows[r])
        if k==RIGHT:return r,(c+1)%len(rows[r])
        nr=(r+(-1 if k==UP else 1))%5
        return nr,min(range(len(rows[nr])),key=lambda nc:abs(self.center(nr,nc)-self.center(r,c)))
    def goto(self,row,col):
        start=(self.row,self.col);q=deque([(start,[])]);seen={start}
        while q:
            pos,path=q.popleft()
            if pos==(row,col):
                for k in path:self.keys.tap(k)
                self.row,self.col=pos;return
            for k in (UP,DOWN,LEFT,RIGHT):
                nxt=self.move(*pos,k)
                if nxt not in seen:seen.add(nxt);q.append((nxt,path+[k]))
        raise AssertionError('unreachable key')
    def switch(self,page):
        if self.page==page:return
        self.keys.tap(R if (page-self.page)%3==1 else L);self.page=page;self.shift=False
        self.col=min(self.col,len(self.rows()[self.row])-1)
    def type(self,text):
        for ch in text:
            if ch==' ':self.goto(4,1);self.keys.tap(A);continue
            found=False
            for page in (self.page,*[p for p in range(3) if p!=self.page]):
                for shift in (False,True):
                    rows=(SHIFT if shift else PAGES)[page]
                    for row,s in enumerate(rows):
                        if ch not in s:continue
                        self.switch(page)
                        if self.shift!=shift:
                            self.goto(4,0);self.keys.tap(A);self.shift=shift
                        self.goto(row,s.index(ch));self.keys.tap(A);found=True;break
                    if found:break
                if found:break
            assert found,ch
        return self
