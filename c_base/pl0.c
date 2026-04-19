/*
 * PL/0 complier program for win32 platform (implemented in C)
 *
 * The program has been test on Visual C++ 6.0, Visual C++.NET and
 * Visual C++.NET 2003, on Win98, WinNT, Win2000, WinXP and Win2003
 *
 * 使用方法：
 * 运行后输入PL/0源程序文件名
 * 回答是否输出虚拟机代码？(Y/N)
 * 回答是否输出符号表？(Y/N)
 * fa.tmp输出虚拟机代码
 * fa1.tmp输出源文件及其各行对应的首地址
 * fa2.tmp输出结果
 * fas.tmp输出符号表
 */

#include <stdio.h>

#include "pl0.h"
#include "string.h"

/*
* ===================== 源码阅读导航（先看这个） =====================
*
* 这份文件可以按“编译前端 -> 代码生成 -> 虚拟机执行”三段来读：
* 1) 词法: getch/getsym
* 2) 语法+语义+生成: block/statement/expression/term/factor/condition + gen
* 3) 解释执行: interpret/base
*
* 强烈建议按下面顺序阅读：
* main -> init -> getsym -> block -> statement -> expression/term/factor -> condition -> interpret -> base
*
* 通读时优先盯住 6 个全局状态：
* sym: 当前记号类型（语法分派开关）
* id/num: 当前标识符或数字字面量
* table/tx: 符号表及其尾指针
* code/cx: 目标指令区及下一条可写位置
* lev: 当前静态作用域层级
* dx: 当前过程的数据区大小游标（从 3 开始，0/1/2 留给 SL/DL/RA）

bool nxtlev[symnum] 等bool数组是把每个词法符号当作一个编号（0 到 symnum-1），数组下标就是符号编号，值 true/false 表示“这个符号是否属于该集合”
*/

/* 解释执行时使用的栈 */
#define stacksize 500


int main()
{
	// 用来存放当前语法分析阶段允许出现的后继符号。
	bool nxtlev[symnum]; // 符号集合”缓冲区（长度是 symnum=32）

	printf("Input pl/0 file?   ");
	scanf("%s", fname);     /* 输入文件名 */

	fin = fopen(fname, "r");

	if (fin)
	{
		printf("List object code?(Y/N)");   /* 是否输出目标代码 */
		scanf("%s", fname);
		listswitch = (fname[0]=='y' || fname[0]=='Y');

		printf("List symbol table?(Y/N)");  /* 是否输出符号表 */
		scanf("%s", fname);
		tableswitch = (fname[0]=='y' || fname[0]=='Y');

		fa1 = fopen("fa1.tmp", "w");
		fprintf(fa1,"Input pl/0 file?   ");
		fprintf(fa1,"%s\n",fname);

		init();     /* 初始化 */

		err = 0;
		cc = cx = ll = 0;
		ch = ' ';

		if(-1 != getsym())
		{
			fa = fopen("fa.tmp", "w");    // 用于输出目标代码
			fas = fopen("fas.tmp", "w");  // 用于输出符号表
			addset(nxtlev, declbegsys, statbegsys, symnum); // 声明开始符号和语句开始符号做并集
			nxtlev[period] = true;  // 把程序结束符 .（period）设为合法

			if(-1 == block(0, 0, nxtlev))   /* 调用编译程序 */
			{
				fclose(fa);
				fclose(fa1);
				fclose(fas);
				fclose(fin);
				printf("\n");
				return 0;
			}
			fclose(fa);
			fclose(fa1);
			fclose(fas);

			if (sym != period)  
			{
				error(9);
			}

			if (err == 0)
			{
				fa2 = fopen("fa2.tmp", "w");
				interpret();    /* 调用解释执行程序 */
				fclose(fa2);
			}
			else
			{
				printf("Errors in pl/0 program");
			}
		}

		fclose(fin);
	}
	else
	{
		printf("Can't open file!\n");
	}

	printf("\n");
	return 0;
}

/*
* 初始化
*/
void init()
{
	int i;

	/* 设置单字符符号 */
	for (i=0; i<=255; i++)
	{
		ssym[i] = nul;
	}
	ssym['+'] = plus;
	ssym['-'] = minus;
	ssym['*'] = times;
	ssym['/'] = slash;
	ssym['('] = lparen;
	ssym[')'] = rparen;
	ssym['='] = eql;
	ssym[','] = comma;
	ssym['.'] = period;
	ssym['#'] = neq;
	ssym[';'] = semicolon;

	/* 设置保留字名字,按照字母顺序，便于折半查找 */
	strcpy(&(word[0][0]), "begin");
	strcpy(&(word[1][0]), "call");
	strcpy(&(word[2][0]), "const");
	strcpy(&(word[3][0]), "do");
	strcpy(&(word[4][0]), "end");
	strcpy(&(word[5][0]), "if");
	strcpy(&(word[6][0]), "odd");
	strcpy(&(word[7][0]), "procedure");
	strcpy(&(word[8][0]), "read");
	strcpy(&(word[9][0]), "then");
	strcpy(&(word[10][0]), "var");
	strcpy(&(word[11][0]), "while");
	strcpy(&(word[12][0]), "write");

	/* 设置保留字符号 */
	wsym[0] = beginsym;
	wsym[1] = callsym;
	wsym[2] = constsym;
	wsym[3] = dosym;
	wsym[4] = endsym;
	wsym[5] = ifsym;
	wsym[6] = oddsym;
	wsym[7] = procsym;
	wsym[8] = readsym;
	wsym[9] = thensym;
	wsym[10] = varsym;
	wsym[11] = whilesym;
	wsym[12] = writesym;

	/* 设置指令名称 */
	strcpy(&(mnemonic[lit][0]), "lit");
	strcpy(&(mnemonic[opr][0]), "opr");
	strcpy(&(mnemonic[lod][0]), "lod");
	strcpy(&(mnemonic[sto][0]), "sto");
	strcpy(&(mnemonic[cal][0]), "cal");
	strcpy(&(mnemonic[inte][0]), "int");
	strcpy(&(mnemonic[jmp][0]), "jmp");
	strcpy(&(mnemonic[jpc][0]), "jpc");

	/* 设置符号集 */
	for (i=0; i<symnum; i++)
	{
		declbegsys[i] = false;
		statbegsys[i] = false;
		facbegsys[i] = false;
	}

	/* 设置声明开始符号集 */
	declbegsys[constsym] = true;
	declbegsys[varsym] = true;
	declbegsys[procsym] = true;

	/* 设置语句开始符号集 */
	statbegsys[beginsym] = true;
	statbegsys[callsym] = true;
	statbegsys[ifsym] = true;
	statbegsys[whilesym] = true;
	statbegsys[readsym] = true;			// thanks to elu
	statbegsys[writesym] = true;

	/* 设置因子开始符号集 */
	facbegsys[ident] = true;
	facbegsys[number] = true;
	facbegsys[lparen] = true;
}

/*
* 用数组实现集合的集合运算
*/
int inset(int e, bool* s)
{
	return s[e];
}

int addset(bool* sr, bool* s1, bool* s2, int n)
{
	int i;
	for (i=0; i<n; i++)
	{
		sr[i] = s1[i]||s2[i];
	}
	return 0;
}

int subset(bool* sr, bool* s1, bool* s2, int n)
{
	int i;
	for (i=0; i<n; i++)
	{
		sr[i] = s1[i]&&(!s2[i]);
	}
	return 0;
}

int mulset(bool* sr, bool* s1, bool* s2, int n)
{
	int i;
	for (i=0; i<n; i++)
	{
		sr[i] = s1[i]&&s2[i];
	}
	return 0;
}

/*
*   出错处理，打印出错位置和错误编码
*/
void error(int n)
{
	char space[81];
	memset(space,32,81);

	space[cc-1]=0; //出错时当前符号已经读完，所以cc-1

	printf("****%s!%d\n", space, n);
	fprintf(fa1,"****%s!%d\n", space, n);

	err++;
}

/*
* 漏掉空格，读取一个字符。
*
* 每次读一行，存入line缓冲区，line被getsym取空后再读一行
*
* 被函数getsym调用。
*/
int getch()
{
	if (cc == ll)
	{
		if (feof(fin))
		{
			printf("program incomplete");
			return -1;
		}
		ll=0;
		cc=0;
		printf("%d ", cx);
		fprintf(fa1,"%d ", cx);
		ch = ' ';
		while (ch != 10)
		{
			//fscanf(fin,"%c", &ch)
			//richard
			if (EOF == fscanf(fin,"%c", &ch))
			{
				line[ll] = 0;
				break;
			}
			//end richard
			printf("%c", ch);
			fprintf(fa1, "%c", ch);
			line[ll] = ch;
			ll++;
		}
		printf("\n");
		fprintf(fa1, "\n");
	}
	ch = line[cc];
	cc++;
	return 0;
}

/*
* 词法分析，获取一个符号
*/
int getsym()
{
	int i,j,k;

	/* the original version lacks "\r", thanks to foolevery */
	while (ch==' ' || ch==10 || ch==13 || ch==9)  /* 忽略空格、换行、回车和TAB */
	{
		getchdo;
	}
	if (ch>='a' && ch<='z')
	{           /* 名字或保留字以a..z开头 */
		k = 0;
		do {
			if(k<al)
			{
				a[k] = ch;
				k++;
			}
			getchdo;
		} while (ch>='a' && ch<='z' || ch>='0' && ch<='9');
		a[k] = 0;
		strcpy(id, a);
		i = 0;
		j = norw-1;
		do {    /* 搜索当前符号是否为保留字 */
			k = (i+j)/2;
			if (strcmp(id,word[k]) <= 0)
			{
				j = k - 1;
			}
			if (strcmp(id,word[k]) >= 0)
			{
				i = k + 1;
			}
		} while (i <= j);
		if (i-1 > j)
		{
			sym = wsym[k];
		}
		else
		{
			sym = ident; /* 搜索失败则，是名字或数字 */
		}
	}
	else
	{
		if (ch>='0' && ch<='9')
		{           /* 检测是否为数字：以0..9开头 */
			k = 0;
			num = 0;
			sym = number;
			do {
				num = 10*num + ch - '0';
				k++;
				getchdo;
			} while (ch>='0' && ch<='9'); /* 获取数字的值 */
			k--;
			if (k > nmax)
			{
				error(30);
			}
		}
		else
		{
			if (ch == ':')      /* 检测赋值符号 */
			{
				getchdo;
				if (ch == '=')
				{
					sym = becomes;
					getchdo;
				}
				else
				{
					sym = nul;  /* 不能识别的符号 */
				}
			}
			else
			{
				if (ch == '<')      /* 检测小于或小于等于符号 */
				{
					getchdo;
					if (ch == '=')
					{
						sym = leq;
						getchdo;
					}
					else
					{
						sym = lss;
					}
				}
				else
				{
					if (ch=='>')        /* 检测大于或大于等于符号 */
					{
						getchdo;
						if (ch == '=')
						{
							sym = geq;
							getchdo;
						}
						else
						{
							sym = gtr;
						}
					}
					else
					{
						sym = ssym[ch];     /* 当符号不满足上述条件时，全部按照单字符符号处理 */
						//getchdo;
						//richard
						if (sym != period)
						{
							getchdo;
						}
						//end richard
					}
				}
			}
		}
	}
	return 0;
}

/*
* 生成虚拟机代码
*
* x: instruction.f;
* y: instruction.l;
* z: instruction.a;
f 是操作码，l 是层差（静态作用域要用），
a 根据 f 不同可以是立即数、地址或者子操作码。
*/
int gen(enum fct x, int y, int z )
{
	if (cx >= cxmax)
	{
		printf("Program too long"); /* 程序过长 */
		return -1;
	}
	code[cx].f = x;
	code[cx].l = y;
	code[cx].a = z;
	cx++;
	return 0;
}


/*
* 测试当前符号是否合法，判错 + 跳到最近可继续的位置
*
* 在某一部分（如一条语句，一个表达式）将要结束时时我们希望下一个符号属于某集合
* （该部分的后跟符号），test负责这项检测，并且负责当检测不通过时的补救措施，
   如果当前点已经错乱了，那至少要跳到一个上层还能接受的位置，程序才能继续分析。
*
* s1:   我们需要的符号
* s2:   如果不是我们需要的，则需要一个补救用的集合
* n:    错误号
*/
int test(bool* s1, bool* s2, int n)
{
	if (!inset(sym, s1))
	{
		error(n);
		/* 当检测不通过时，不停获取符号，直到它属于需要的集合或补救的集合 */
		while ((!inset(sym,s1)) && (!inset(sym,s2)))
		{
			getsymdo;
		}
	}
	return 0;
}

/*
* 编译程序主体
*
* lev:    当前分程序所在层，告诉编译器“这个名字属于哪一层作用域”，以便正确处理访问和分配内存
* tx:     符号表当前尾指针，每声明一个新名字（常量/变量/过程），都会在符号表新增一行，tx 往后走
* fsys:   当前模块后的符号集合
*/
int block(int lev, int tx, bool* fsys)
{
	int i;

	int dx;                 /* 名字分配到的相对地址 */
	int tx0;                /* 保留初始tx */
	int cx0;                /* 保留初始cx  */
	bool nxtlev[symnum];    /* 当前允许出现的后继符号集合*/

	dx = 3;					/*每个分程序的前3个地址分别存放静态链、动态链和返回地址
							SL，静态链 含义：指向“词法上外一层过程”的栈帧基址。 用途：访问外层变量。(写程序时就定)
							DL，动态链 含义：指向"调用该过程"的栈帧基址。 用途：返回时恢复调用者环境。(执行时才知道)
							RA，返回地址 含义：调用点之后下一条要执行的指令地址。 用途：返回时跳转到该地址继续执行。(执行时才知道)
							*/

	tx0 = tx;               /* 记录本层名字的初始位置 */
	table[tx].adr = cx;

	gendo(jmp, 0, 0);    // 生成跳转指令，跳过声明部分，直接执行代码部分，等声明部分处理完了再回填地址

	if (lev > levmax)
	{
		error(32);
	}

	/*
	 * 声明区循环，等价于：
	 * [const 声明][var 声明]{procedure 声明}
	 * 通过 sym 是否属于 declbegsys 决定是否继续吸收声明。
	 */
	do {

		if (sym == constsym)    /* 收到常量声明符号，开始处理常量声明 */
		{
			getsymdo;

			/* the original do...while(sym == ident) is problematic, thanks to calculous */
			/* do { */
			constdeclarationdo(&tx, lev, &dx);  /* dx的值会被constdeclaration改变，使用指针 */
			while (sym == comma)  // 只要后面还有逗号，就继续读后续常量项
			{
				getsymdo;
				constdeclarationdo(&tx, lev, &dx);  //  先吃掉逗号，再解析下一个 ident = number。
			}
			if (sym == semicolon)
			{
				getsymdo;
			}
			else
			{
				error(5);   /*漏掉了逗号或者分号*/
			}
			/* } while (sym == ident); */
		}
		// 为什么下面用第二个 if（不是 else if）：因为同一轮里能接着吃 var 声明
		if (sym == varsym)      /* 收到变量声明符号，开始处理变量声明 */
		{
			getsymdo;

			/* the original do...while(sym == ident) is problematic, thanks to calculous */
			/* do {  */
			vardeclarationdo(&tx, lev, &dx);
			while (sym == comma)
			{
				getsymdo;
				vardeclarationdo(&tx, lev, &dx);
			}
			if (sym == semicolon)
			{
				getsymdo;
			}
			else
			{
				error(5);
			}
			/* } while (sym == ident);  */
		}

		while (sym == procsym) /* 收到过程声明符号，开始处理过程声明 */
		{
			getsymdo;

			if (sym == ident)
			{
				enter(procedur, &tx, lev, &dx); /* 记录过程名字 */
				getsymdo;
			}
			else
			{
				error(4);   /* procedure后应为标识符 */
			}

			if (sym == semicolon)
			{
				getsymdo;
			}
			else
			{
				error(5);   /* 漏掉了分号 */
			}

			memcpy(nxtlev, fsys, sizeof(bool)*symnum);
			nxtlev[semicolon] = true;
			if (-1 == block(lev+1, tx, nxtlev))
			{
				return -1;  /* 递归调用 */
			}
			// 边界校验 + 出错恢复
			if(sym == semicolon)
			{
				getsymdo;
				memcpy(nxtlev, statbegsys, sizeof(bool)*symnum);
				nxtlev[ident] = true;
				nxtlev[procsym] = true;
				testdo(nxtlev, fsys, 6);
			}
			else
			{
				error(5);   /* 漏掉了分号 */
			}
		}
		memcpy(nxtlev, statbegsys, sizeof(bool)*symnum);  // 先继承上层允许的后继符号, 出错时至少能回到“父层还能接受”的位置。
		nxtlev[ident] = true;							// 再补本层额外合法符号, Follow(子结构)=Follow(父结构)∪本地分隔符
		testdo(nxtlev, declbegsys, 7);					// 最后做校验与恢复, 如果当前符号既不是声明开始符号，也不是语句开始符号，则出错，并且不停获取符号直到它属于其中之一。
	} while (inset(sym, declbegsys));   /* 直到没有声明符号 */

	/* 回填开头 jmp 的目标地址，使其跳过声明区，直接进入可执行语句区。 */
	code[table[tx0].adr].a = cx;    /* 回填之前那条 jmp 的目标，让它跳到当前 cx（也就是语句区入口） */
	table[tx0].adr = cx;            /* 把当前过程在符号表里的入口地址也记成 cx */
	table[tx0].size = dx;           /* 记录这个过程总共需要的栈帧大小（含 SL/DL/RA + 局部变量） */
	cx0 = cx;                       // 记住这一段代码的起点，后面用于 listcode 输出
	gendo(inte, 0, dx);             /* 生成分配内存代码 */

	if (tableswitch)        /* 输出符号表 */
	{
		printf("TABLE:\n");
		if (tx0+1 > tx)
		{
			printf("    NULL\n");
		}
		for (i=tx0+1; i<=tx; i++)
		{
			switch (table[i].kind)
			{
			case constant:
				printf("    %d const %s ", i, table[i].name);
				printf("val=%d\n", table[i].val);
				fprintf(fas, "    %d const %s ", i, table[i].name);
				fprintf(fas, "val=%d\n", table[i].val);
				break;
			case variable:
				printf("    %d var   %s ", i, table[i].name);
				printf("lev=%d addr=%d\n", table[i].level, table[i].adr);
				fprintf(fas, "    %d var   %s ", i, table[i].name);
				fprintf(fas, "lev=%d addr=%d\n", table[i].level, table[i].adr);
				break;
			case procedur:
				printf("    %d proc  %s ", i, table[i].name);
				printf("lev=%d addr=%d size=%d\n", table[i].level, table[i].adr, table[i].size);
				fprintf(fas,"    %d proc  %s ", i, table[i].name);
				fprintf(fas,"lev=%d addr=%d size=%d\n", table[i].level, table[i].adr, table[i].size);
				break;
			}
		}
		printf("\n");
	}

	/* 语句后跟符号为分号或end */
	memcpy(nxtlev, fsys, sizeof(bool)*symnum);  /* 把上层允许的后继符号先拷过来，保证出错时还能同步回父层。 */
	nxtlev[semicolon] = true;
	nxtlev[endsym] = true;
	statementdo(nxtlev, &tx, lev);			// 真正去解析并生成“可执行语句部分”的代码。
	gendo(opr, 0, 0);                       /* 每个过程出口都要使用的释放数据段指令 */
	memset(nxtlev, 0, sizeof(bool)*symnum); /*分程序没有补救集合 */
	testdo(fsys, nxtlev, 8);                /* 检测后跟符号正确性 */
	listcode(cx0);                          /* 输出代码 */
	return 0;
}

/*
* 在符号表中加入一项
* k:      名字种类const,var or procedure
* ptx:    符号表尾指针的指针，为了可以改变符号表尾指针的值
* lev:    名字所在的层次,，以后所有的lev都是这样
* pdx:    dx为当前应分配的变量的相对地址，分配后要增加1
*/
void enter(enum object k, int* ptx, int lev, int* pdx)
{
	(*ptx)++;
	strcpy(table[(*ptx)].name, id); /* 全局变量id中已存有当前名字的名字 */
	table[(*ptx)].kind = k;
	switch (k)
	{
	case constant:  /* 常量名字 */
		if (num > amax)
		{
			error(31);  /* 数越界 */
			num = 0;
		}
		table[(*ptx)].val = num;
		break;
	case variable:  /* 变量名字 */
		table[(*ptx)].level = lev;
		table[(*ptx)].adr = (*pdx);
		(*pdx)++;
		break;
	case procedur:  /*　过程名字　*/
		table[(*ptx)].level = lev;
		break;
	}
}

/*
* 查找名字的位置.
* 找到则返回在符号表中的位置,否则返回0.
* idt:    要查找的名字
* tx:     当前符号表尾指针
*/
int position(char* idt, int tx)
{
	int i;
	// 先把要找的名字放到 table[0]，这样搜索一定会停下来，不用额外判断边界。
	strcpy(table[0].name, idt);
	i = tx;
	while (strcmp(table[i].name, idt) != 0)
	{
		i--;
	}
	return i;
}

/*
* 常量声明处理
*/
int constdeclaration(int* ptx, int lev, int* pdx)
{
	/* const 只写符号表的值域，不会像 var 一样消耗运行时地址槽位。 */
	if (sym == ident)
	{
		getsymdo;
		if (sym==eql || sym==becomes)
		{
			if (sym == becomes)
			{
				error(1);   /* 把=写成了:= */
			}
			getsymdo;
			if (sym == number)
			{
				enter(constant, ptx, lev, pdx);
				getsymdo;
			}
			else
			{
				error(2);   /* 常量说明=后应是数字 */
			}
		}
		else
		{
			error(3);   /* 常量说明标识后应是= */
		}
	}
	else
	{
		error(4);   /* const后应是标识 */
	}
	return 0;
}

/*
* 变量声明处理
*/
int vardeclaration(int* ptx,int lev,int* pdx)
{
	if (sym == ident)
	{
		enter(variable, ptx, lev, pdx); // 填写符号表
		getsymdo;
	}
	else
	{
		error(4);   /* var后应是标识 */
	}
	return 0;
}

/*
* 输出目标代码清单
*/
void listcode(int cx0)
{
	int i;
	if (listswitch)
	{
		for (i=cx0; i<cx; i++)
		{
			printf("%d %s %d %d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
			fprintf(fa,"%d %s %d %d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
		}
	}
}

/*
* 语句处理
*/
int statement(bool* fsys, int* ptx, int lev)
{
	int i, cx1, cx2;
	bool nxtlev[symnum];

	/*
	 * 语句分派核心：根据当前 sym 进入不同分支。
	 * 对应文法：
	 * statement := ident:=... | read(...) | write(...) | call ident
	 *            | if ... then ... | begin ... end | while ... do ...
	 */

	if (sym == ident)   /* 准备按照赋值语句处理 */
	{
		i = position(id, *ptx);
		if (i == 0)
		{
			error(11);  /* 变量未找到 */
		}
		else
		{
			if(table[i].kind != variable)
			{
				error(12);  /* 赋值语句格式错误 */
				i = 0;
			}
			else
			{
				getsymdo;
				if(sym == becomes)
				{
					getsymdo;
				}
				else
				{
					error(13);  /* 没有检测到赋值符号 */
				}
				memcpy(nxtlev, fsys, sizeof(bool)*symnum);
				expressiondo(nxtlev, ptx, lev); /* 处理赋值符号右侧表达式 */
				if(i != 0)
				{
					/* expression将执行一系列指令，但最终结果将会保存在栈顶，执行sto命令完成赋值 */
					gendo(sto, lev-table[i].level, table[i].adr);
				}
			}
		}//if (i == 0)
	}
	else
	{
		if (sym == readsym) /* 准备按照read语句处理 */
		{
			getsymdo;
			if (sym != lparen)
			{
				error(34);  /* 格式错误，应是左括号 */
			}
			else
			{
				do {
					getsymdo;
					if (sym == ident)
					{
						i = position(id, *ptx); /* 查找要读的变量 */
					}
					else
					{
						i=0;
					}

					if (i == 0)
					{
						error(35);  /* read()中应是声明过的变量名 */
					}
					else if (table[i].kind != variable)
					{
						error(32);	/* read()参数表的标识符不是变量, thanks to amd */
					}
					else
					{
						gendo(opr, 0, 16);  /* 生成输入指令，读取值到栈顶 */
						gendo(sto, lev-table[i].level, table[i].adr);   /* 储存到变量 */
					}
					getsymdo;

				} while (sym == comma); /* 一条read语句可读多个变量 */
			}
			if(sym != rparen)
			{
				error(33);  /* 格式错误，应是右括号 */
				while (!inset(sym, fsys))   /* 出错补救，直到收到上层函数的后跟符号 */
				{
					getsymdo;
				}
			}
			else
			{
				getsymdo;
			}
		}
		else
		{
			if (sym == writesym)    /* 准备按照write语句处理，与read类似 */
			{
				getsymdo;
				if (sym == lparen)
				{
					do {
						getsymdo;
						memcpy(nxtlev, fsys, sizeof(bool)*symnum);
						nxtlev[rparen] = true;
						nxtlev[comma] = true;       /* write的后跟符号为) or , */
						expressiondo(nxtlev, ptx, lev); /* 调用表达式处理，此处与read不同，read为给变量赋值 */
						gendo(opr, 0, 14);  /* 生成输出指令，输出栈顶的值 */
					} while (sym == comma);
					if (sym != rparen)
					{
						error(33);  /* write()中应为完整表达式 */
					}
					else
					{
						getsymdo;
					}
				}
				gendo(opr, 0, 15);  /* 输出换行 */
			}
			else
			{
				if (sym == callsym) /* 准备按照call语句处理 */
				{
					getsymdo;
					if (sym != ident)
					{
						error(14);  /* call后应为标识符 */
					}
					else
					{
						i = position(id, *ptx);
						if (i == 0)
						{
							error(11);  /* 过程未找到 */
						}
						else
						{
							if (table[i].kind == procedur)
							{
								gendo(cal, lev-table[i].level, table[i].adr);   /* 生成call指令 */
							}
							else
							{
								error(15);  /* call后标识符应为过程 */
							}
						}
						getsymdo;
					}
				}
				else
				{
					if (sym == ifsym)   /* 准备按照if语句处理 */
					{
						getsymdo;
						// 用 memcpy 复制父层 fsys，再按当前语法补几个合法符号
						memcpy(nxtlev, fsys, sizeof(bool)*symnum);
						nxtlev[thensym] = true;
						nxtlev[dosym] = true;   /* 后跟符号为then或do */
						conditiondo(nxtlev, ptx, lev); /* 调用条件处理（逻辑运算）函数 */
						if (sym == thensym)
						{
							getsymdo;
						}
						else
						{
							error(16);  /* 缺少then */
						}
						cx1 = cx;   /* 保存当前指令地址 */
						/* 先占位一条 jpc，待 then 语句生成完后再回填跳转目标。 */
						gendo(jpc, 0, 0);   /* 生成条件跳转指令，跳转地址未知，暂时写0 */
						statementdo(fsys, ptx, lev);    /* 处理then后的语句 */
						code[cx1].a = cx;   /* 经statement处理后，cx为then后语句执行完的位置，它正是前面未定的跳转地址 */
					}
					else
					{
						if (sym == beginsym)    /* 准备按照复合语句处理 */
						{
							getsymdo;
							memcpy(nxtlev, fsys, sizeof(bool)*symnum);
							nxtlev[semicolon] = true;
							nxtlev[endsym] = true;  /* 后跟符号为分号或end */
							/* 循环调用语句处理函数，直到下一个符号不是语句开始符号或收到end */
							statementdo(nxtlev, ptx, lev);

							while (inset(sym, statbegsys) || sym==semicolon)
							{
								if (sym == semicolon)
								{
									getsymdo;
								}
								else
								{
									error(10);  /* 缺少分号 */
								}
								statementdo(nxtlev, ptx, lev);
							}
							if(sym == endsym)
							{
								getsymdo;
							}
							else
							{
								error(17);  /* 缺少end或分号 */
							}
						}
						else
						{
							if (sym == whilesym)    /* 准备按照while语句处理 */
							{
								cx1 = cx;   /* 保存判断条件操作的位置 */
								getsymdo;
								memcpy(nxtlev, fsys, sizeof(bool)*symnum);
								nxtlev[dosym] = true;   /* 后跟符号为do */
								conditiondo(nxtlev, ptx, lev);  /* 调用条件处理 */
								cx2 = cx;   /* 保存循环体的结束的下一个位置 */
								/* while 同样采用回填：条件假时跳出地址先写 0，循环体结束后再回填。 */
								gendo(jpc, 0, 0);   /* 生成条件跳转，但跳出循环的地址未知 */
								if (sym == dosym)
								{
									getsymdo;
								}
								else
								{
									error(18);  /* 缺少do */
								}
								statementdo(fsys, ptx, lev);    /* 循环体 */
								gendo(jmp, 0, cx1); /* 回头重新判断条件 */
								code[cx2].a = cx;   /* 反填跳出循环的地址，与if类似 */
							}
							else
							{
								memset(nxtlev, 0, sizeof(bool)*symnum); /* 语句结束无补救集合 */
								testdo(fsys, nxtlev, 19);   /* 检测语句结束的正确性 */
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

/*
* 表达式处理
*/
int expression(bool* fsys, int* ptx, int lev)
{
	enum symbol addop;  /* 用于保存正负号 */
	bool nxtlev[symnum];
	/* expression := [ + | - ] term { ( + | - ) term } */

	if(sym==plus || sym==minus) /* 开头的正负号，此时当前表达式被看作一个正的或负的项 */
	{
		addop = sym;    /* 保存开头的正负号 */
		getsymdo;
		memcpy(nxtlev, fsys, sizeof(bool)*symnum);
		nxtlev[plus] = true;
		nxtlev[minus] = true;
		termdo(nxtlev, ptx, lev);   /* 处理项 */
		if (addop == minus)
		{
			gendo(opr,0,1); /* 如果开头为负号生成取负指令 */
		}
	}
	else    /* 此时表达式被看作项的加减 */
	{
		memcpy(nxtlev, fsys, sizeof(bool)*symnum);
		nxtlev[plus] = true;
		nxtlev[minus] = true;
		termdo(nxtlev, ptx, lev);   /* 处理项 */
	}
	while (sym==plus || sym==minus)
	{
		addop = sym;
		getsymdo;
		memcpy(nxtlev, fsys, sizeof(bool)*symnum);
		nxtlev[plus] = true;
		nxtlev[minus] = true;
		termdo(nxtlev, ptx, lev);   /* 处理项 */
		if (addop == plus)
		{
			gendo(opr, 0, 2);   /* 生成加法指令 */
		}
		else
		{
			gendo(opr, 0, 3);   /* 生成减法指令 */
		}
	}
	return 0;
}

/*
* 项处理:处理乘除
*/
int term(bool* fsys, int* ptx, int lev)
{
	enum symbol mulop;  /* 用于保存乘除法符号 */
	bool nxtlev[symnum];
	/* term := factor { ( * | / ) factor } */

	memcpy(nxtlev, fsys, sizeof(bool)*symnum);
	nxtlev[times] = true;
	nxtlev[slash] = true;
	factordo(nxtlev, ptx, lev); /* 处理因子 */
	while(sym==times || sym==slash)
	{
		mulop = sym;
		getsymdo;
		factordo(nxtlev, ptx, lev);
		if(mulop == times)
		{
			gendo(opr, 0, 4);   /* 生成乘法指令 */
		}
		else
		{
			gendo(opr, 0, 5);   /* 生成除法指令 */
		}
	}
	return 0;
}

/*
* 因子处理
*/
int factor(bool* fsys, int* ptx, int lev)
{
	int i;
	bool nxtlev[symnum];
	/* factor := ident | number | '(' expression ')' */
	testdo(facbegsys, fsys, 24);    /* 检测因子的开始符号 */
	/* while(inset(sym, facbegsys)) */  /* 循环直到不是因子开始符号 */
	if(inset(sym,facbegsys))    /* BUG: 原来的方法var1(var2+var3)会被错误识别为因子 */
	{
		if(sym == ident)    /* 因子为常量或变量 */
		{
			i = position(id, *ptx); /* 查找名字 */
			if (i == 0)
			{
				error(11);  /* 标识符未声明 */
			}
			else
			{
				switch (table[i].kind)
				{
				case constant:  /* 名字为常量 */
					gendo(lit, 0, table[i].val);    /* 直接把常量的值入栈 */
					break;
				case variable:  /* 名字为变量 */
					gendo(lod, lev-table[i].level, table[i].adr);   /* 找到变量地址并将其值入栈 */
					break;
				case procedur:  /* 名字为过程 */
					error(21);  /* 不能为过程 */
					break;
				}
			}
			getsymdo;
		}
		else
		{
			if(sym == number)   /* 因子为数 */
			{
				if (num > amax)
				{
					error(31);
					num = 0;
				}
				gendo(lit, 0, num);
				getsymdo;
			}
			else
			{
				if (sym == lparen)  /* 因子为表达式 */
				{
					getsymdo;
					memcpy(nxtlev, fsys, sizeof(bool)*symnum);
					nxtlev[rparen] = true;
					expressiondo(nxtlev, ptx, lev);
					if (sym == rparen)
					{
						getsymdo;
					}
					else
					{
						error(22);  /* 缺少右括号 */
					}
				}
				testdo(fsys, facbegsys, 23);    /* 因子后有非法符号 */
			}
		}
	}
	return 0;
}

/*
* 条件处理
*/
int condition(bool* fsys, int* ptx, int lev)
{
	enum symbol relop;
	bool nxtlev[symnum];
	/* condition := odd expression | expression relop expression */

	if(sym == oddsym)   /* 准备按照odd运算处理 */
	{
		getsymdo;
		expressiondo(fsys, ptx, lev);
		gendo(opr, 0, 6);   /* 生成odd指令 */
	}
	else
	{
		/* 逻辑表达式处理 */
		memcpy(nxtlev, fsys, sizeof(bool)*symnum);
		nxtlev[eql] = true;
		nxtlev[neq] = true;
		nxtlev[lss] = true;
		nxtlev[leq] = true;
		nxtlev[gtr] = true;
		nxtlev[geq] = true;
		expressiondo(nxtlev, ptx, lev);
		if (sym!=eql && sym!=neq && sym!=lss && sym!=leq && sym!=gtr && sym!=geq)
		{
			error(20);
		}
		else
		{
			relop = sym;
			getsymdo;
			expressiondo(fsys, ptx, lev);
			switch (relop)
			{
			case eql:
				gendo(opr, 0, 8);
				break;
			case neq:
				gendo(opr, 0, 9);
				break;
			case lss:
				gendo(opr, 0, 10);
				break;
			case geq:
				gendo(opr, 0, 11);
				break;
			case gtr:
				gendo(opr, 0, 12);
				break;
			case leq:
				gendo(opr, 0, 13);
				break;
			}
		}
	}
	return 0;
}

/*
* 解释程序
*/
void interpret()
{
	int p, b, t;    /* 指令指针，指令基址，下一空槽位指针 */
	struct instruction i;   /* 存放当前指令 */
	int s[stacksize];   /* 栈 */
	/*
	 * 栈帧布局（以 b 为当前过程基址）：
	 * s[b+0] = 静态链 SL
	 * s[b+1] = 动态链 DL
	 * s[b+2] = 返回地址 RA
	 * s[b+3...] = 局部数据区
	 */

	printf("start pl0\n");
	t = 0;
	b = 0;
	p = 0;
	s[0] = s[1] = s[2] = 0;
	do { // 这台虚拟机把 t 设计成“下一空槽位指针”，也就是栈顶元素的下一个位置，这样在执行入栈操作时就不需要先增加 t 了。
		/* 取指 -> 译码 -> 执行 */
		i = code[p];    /* 读当前指令 */
		p++;
		switch (i.f)
		{
		case lit:   /* 将a的值取到栈顶 */
			s[t] = i.a;
			t++;
			break;
		case opr:   /* 按照a指定的操作进行数学、逻辑运算 */
			switch (i.a)
			{
			case 0:  // 过程返回（恢复 p、b）
				t = b;
				p = s[t+2];
				b = s[t+1];
				break;
			case 1:  // 取负
				s[t-1] = -s[t-1];
				break;
			case 2:  
				t--;  // 二元运算前先把 t 减 1，这样 s[t] 就是右操作数，s[t-1] 是左操作数，结果直接覆盖 s[t-1]。
				s[t-1] = s[t-1]+s[t];
				break;
			case 3:
				t--;
				s[t-1] = s[t-1]-s[t];
				break;
			case 4:
				t--;
				s[t-1] = s[t-1]*s[t];
				break;
			case 5:
				t--;
				s[t-1] = s[t-1]/s[t];
				break;
			case 6:  // odd
				s[t-1] = s[t-1]%2;
				break;
			case 8:  // 等于eql
				t--;
				s[t-1] = (s[t-1] == s[t]);
				break;
			case 9:  // 不等于neq
				t--;
				s[t-1] = (s[t-1] != s[t]);
				break;
			case 10:  // 小于lss
				t--;
				s[t-1] = (s[t-1] < s[t]);
				break;
			case 11:  // 大于等于geq
				t--;
				s[t-1] = (s[t-1] >= s[t]);
				break;
			case 12:  // 大于gtr
				t--;
				s[t-1] = (s[t-1] > s[t]);
				break;
			case 13:  // 小于等于leq
				t--;
				s[t-1] = (s[t-1] <= s[t]);
				break;
			case 14:  // 输出栈顶的值
				printf("%d", s[t-1]);
				fprintf(fa2, "%d", s[t-1]);
				t--;
				break;
			case 15:  // 输出换行
				printf("\n");
				fprintf(fa2,"\n");
				break;
			case 16:  // 从输入读一个数到栈顶
				printf("?");
				fprintf(fa2, "?");
				scanf("%d", &(s[t]));
				fprintf(fa2, "%d\n", s[t]);
				t++;
				break;
			}
			break;
		case lod:   /* 取相对当前过程的数据基地址为a的内存的值到栈顶 */
			s[t] = s[base(i.l,s,b)+i.a];
			t++;
			break;
		case sto:   /* 栈顶的值存到相对当前过程的数据基地址为a的内存 */
			t--;
			s[base(i.l, s, b) + i.a] = s[t];
			break;
		case cal:   /* cal l, a, 调用位于a的过程，层次差为l*/
			s[t] = base(i.l, s, b); /* 将父过程基地址入栈 */
			s[t+1] = b; 			/* 将本过程基地址入栈 */
			s[t+2] = p; 			/* 将当前指令指针入栈 */
			b = t;  				/* 改变基地址指针值为新过程的基地址 */
			p = i.a;   				/* 跳到子过程入口地址 */
			break;
		case inte:  /* inte 0, a：分配a个内存 */
			t += i.a;
			break;
		case jmp:   /* jmp 0, a：无条件跳转到a */
			p = i.a;
			break;
		case jpc:   /* jpc 0, a：条件跳转到a（当栈顶值为0时） */
			t--;
			if (s[t] == 0)
			{
				p = i.a;
			}
			break;
		}
	} while (p != 0);
}

/* 沿着静态链往外跳 l 层，找到目标作用域的基址。 */
int base(int l, int* s, int b)
{
	int b1;
	b1 = b;  // 先把 b1 设为当前过程基址 b
	while (l > 0)
	{
		b1 = s[b1];  // 沿着静态链向外跳一层，s[b1] 就是上一层过程的基址
		l--;
	}
	return b1;
}