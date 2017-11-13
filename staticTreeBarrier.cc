//#include <thread>
#include <threads.h>
#include <atomic>
#include <iostream>
#include <librace.h>

using namespace std;

class staticTreeBarrier
{
private:
	class treeNode
	{
	private:
		atomic_int childNum;
		treeNode * parent;
		int children;
	public:
		void setParent(treeNode * newParent, int count)
		{
			children = count;
			parent = newParent;
			childNum.store(count, memory_order_relaxed);
		}
		treeNode()
		{
			children=0;
			childNum.store(0,memory_order_relaxed);
			parent = NULL;
		}
		void await(staticTreeBarrier* outBarrier)
		{
			bool nodeSense;
			nodeSense = outBarrier->mySense.load(memory_order_relaxed);
			while (childNum.load(memory_order_acquire) > 0)
			{
				//this_thread::yield();
				thrd_yield();
			}
			childNum.store(children, memory_order_relaxed);
			if (parent != NULL)
			{
				parent->childDone();
				//parent->childNum.fetch_sub(1,memory_order_seq_cst);
				while (nodeSense != outBarrier->sense.load(memory_order_acquire))
				{
					//this_thread::yield();
					thrd_yield();
				}
			}
			else
			{
				outBarrier->sense.store(nodeSense,memory_order_release);	
							
			}
			outBarrier->mySense.store(!nodeSense, memory_order_relaxed);
			
		}
		void childDone()
		{
			//while (childNum.load(memory_order_acquire)==0);
			childNum.fetch_sub(1,memory_order_release);
		}
		~treeNode() {}
	};
	const int radix;
	atomic_bool sense;
	atomic_bool mySense;
	treeNode **node;
	int nodes;
	int treeSize;
	void build(treeNode *parent,int depth)
	{
		int nodepoint;
		if (depth == 0)
			node[nodes++]->setParent(parent, 0);
		else
		{
			nodepoint = nodes;
			node[nodes++]->setParent(parent,radix);
			for (int i = 0; i < radix; i++)
				build(node[nodepoint], depth - 1);
		}
	}
public:
	staticTreeBarrier(int size, int newRadix):radix(newRadix)
	{
		treeSize=size;
		node = new treeNode*[size];
		for (int i=0;i<size;i++)
			node[i]=new treeNode();
		nodes = 0;
		int depth = 0;
		while (size > 1)
		{
			depth++;
			size /= radix;
		}
		build(NULL, depth);
		sense.store (false,memory_order_relaxed);
		mySense.store(true, memory_order_relaxed);
	}
	void await(int threadNo)
	{
		node[threadNo]->await(this);
	}
	~staticTreeBarrier() 
	{ 
		for (int i=0;i<treeSize;i++)
			delete node[i];
		delete [] node;
 		//node = 0; 
	}
};

//test part 
//------global variables and pointer for the test
staticTreeBarrier *mystb;
int testInt;
void (*test)(void*);
//-------------------------


void test1(void* arg)
{
	int localInt1;
	int *tn=(int*)arg;
	if (*tn==0)
		store_32(&testInt,10);
	mystb->await(*tn);
	localInt1=load_32(&testInt);
}
void test2(void *arg)
{
	int localInt1;
	int localInt2;
	int *tn=(int*)arg;
	if (*tn==0)
		store_32(&testInt,10);
	mystb->await(*tn);
	localInt1=load_32(&testInt);
	mystb->await(*tn);
	if (*tn==0)
		store_32(&testInt,20);
	mystb->await(*tn);
	localInt2=load_32(&testInt);
}	
//int main()
int user_main(int a, char** b)
{
	//staticTreeBarrier *mystb;
	mystb= new staticTreeBarrier(3, 2);
	thrd_t t1;
	thrd_t t2;
	thrd_t t3;
	int res1=0;
	int res2=1;
	int res3=2;
	test=&test1;
	//test=&test2;
	thrd_create(&t1,test,&res1);
	thrd_create(&t2,test,&res2);
	thrd_create(&t3,test,&res3);
	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	delete mystb;
	return 0;
}
