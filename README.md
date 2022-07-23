# week1 
杨博欣：由于期末考等因素，本周暂时只完成实验环境的配置，后续将根据现有最新研究工作，确定我们的优化目标和优化思路。

汪旭：由于本周为考试周，暂时只完成实验环境的配置，通过阅读实验的背景资料，了解了FUSE文件系统的工作方式。



# 7/11


通过对F2FS的了解得知，F2FS是一个Log-structured File System(LFS)，因此会使用异地更新策略。假设有一个文件，它的文件数据保存在物理地址100的位置中，此时用户对文件内容进行更新:
非LFS: 使用就地更新策略，将更新后的数据写入到物理地址100中。
LFS: 使用异地更新策略，首先会分配一个新的物理地址101，然后将数据写入新物理地址101中，接着将文件指针指向新的物理地址101，最后将旧的物理地址100进行回收再利用。
这种设计的好处是:
可以将随机写转换为顺序写以获得更好的性能提升;
flash的颗粒program寿命是有限的，通过LFS的异地更新特性，可以自带磨损均衡。
但是LFS也有一些缺点，一个最明显的缺点是F2FS要对旧的物理地址进行回收，这个过程称为垃圾回收过程(GC)，不适当的GC时机会影响到系统的性能表现，而这也是我们打算进行优化的影响因素；另外一个缺点是LFS极端情况的安全性不像JFS(journal file system)那么好，因为LFS依赖Checkpoint保证一致性，但是Checkpoint不是每次写入数据都会进行(带来很大的开销)，而是隔一段时间才会进行一次Checkpoint，因此可能在Checkpoint之前系统宕机，会带来部分数据的丢失。

在F2FS中，冷热数据分区。其规则是：Node比Data热，在Node中，目录文件下的Direct Node为Hot、File文件下的Direct Node为Warm、Indirect Node为Cold；Data中，Dirent为Hot、普通Data Block为Warm、被回收的、用户指定的、多媒体数据文件等为Cold。这种分类较为简单，这也是我们可能在实际应用环境中取得进展的地方。同时F2FS使用NAT减少更新传播（update propagation），所有Node通过NAT（Node Address Table） 翻译。

不像LFS只有一个大的log区域，F2FS维护了六个主要的log区域来最大化冷热数据分区效应。F2FS静态地为Node Block和Data Block定义了3个数据温度级别——hot、warm以及cold。F2FS默认开启6个Log区域。同样也支持2、4个Log区域。当6个区域都被激活时，6个区域对应上表的6种温度级别的Block；当4个区域被激活时，F2FS将Cold和Warm Log区域合并在一起。如果只有2个Log区域，那么F2FS将一个区域分配给Node Block，一个区域分配给Data Block。
F2FS提供了一种Zone可定制FTL兼容方法，能够间接减轻GC开销。具体原理不太清楚

在我的理解中，冷热数据分离的原因应该是基于性能和磨损平衡两个方面的考量，热数据可以挪动。
有人提出对操作系统触发的写入和垃圾收集算法触发的写入使用不同块的写入方法。这些方法不需要数据识别技术，从而简化了设备的设计，同时也减少了写入放大。
使用平均场模型和模拟实验比较这种写入方法与依赖冷热数据识别的写入方法的性能。主要发现是识别热数据和冷数据的额外收益非常有限，尤其是当热数据变得更热时。此外，如果标记为热的数据的比例没有理想地选择，或者在识别数据时出现误报或误报的可能性很大（例如 5%），则依赖于热数据和冷数据识别的写入方法甚至可能会变得很差。在识别这个方面，我们在本次研究中也会考虑对其优化的可行性。

明天计划了解F2FS垃圾回收具体机制并对其源码进行分析。



# 7/12  7/13
通过阅读F2FS GC部分源码，得知
F2FS的GC分为前台GC和后台GC: 前台GC一般在系统空间紧张的情况下运行，目的是尽快回收空间; 而后台GC则是在系统空闲的情况下进行，目的是在不影响用户体验的情况回收一定的空间。前台GC一般情况下是在checkpoint或者写流程的时候触发，因为F2FS能够感知空间的使用率，如果空间不够了会常触发前台GC加快回收空间，这意味着文件系统空间不足的时候，性能可能会下降。后台GC则是被一个线程间隔一段时间进行触发。关键在于后台GC。
源码表示gc的触发间隔会根据实际情况进行变化。
increase_sleep_time以及decrease_sleep_time是调整gc间隔的函数，其中入参long *wait即为gc的间隔时间，而指针类型的原因是等待时间可以在该函数进行变化。

对于increase_sleep_time函数而言，如果目前的等待时间等于no_gc_sleep_time，则不做变化，表示系统处于不需要频繁做后台GC的情况，继续维持这种状态。如果不是，则增加30秒的gc间隔时间。

对于decrease_sleep_time函数而言，如果目前的等待时间等于no_gc_sleep_time，则不做变化，表示系统处于不需要频繁做后台GC的情况，继续维持这种状态。如果不是，则减少30秒的gc间隔时间。
f2fs的最小单位是block，往上的是segment，再往上是section，最高是zone。gc的回收单位是section，在默认情况下，一个section等于一个segment，因此每回收一个section就回收了512个block。

从gc线程主函数gc_thread_func可以知道，gc的核心是f2fs_gc函数的执行。

do_garbage_collect函数是gc流程的主要操作，作用是根据入参的段号segno找到对应的segment，然后将整个segment读取出来，通过异地更新的方式写入迁移到其他segment中。这样操作以后，被gc的segment会变为一个全新的segment进而可以被系统重新使用。

F2FS通过f2fs_summary_block结构，根据物理地址找到对应的inode，每一个segment都对应一个f2fs_summary_block结构。segment中每一个block都对应了f2fs_summary_block结构中的一个entry，记录了这个block地址属于哪个node(通过node id)以及属于这个node的第几个block。



# 7/14
通过阅读F2FS GC数据迁移部分源码，明白了数据迁移以及建立inode list去管理被迁移segment中valid block的详细过程。
第一阶段(phase=0): 根据entry记录的nid，通过ra_node_page函数可以将这个nid对应的node page读入到内存当中。

第二阶段(phase=1): 根据start_addr以及entry，通过check_dnode函数，找到了对应的struct node_info *dni，它记录这个block是属于哪一个inode(inode no)，然后将对应的inode page读入到内存当中。

第三阶段(phase=2): 首先通过entry->ofs_in_node获取到当前block属于node的第几个block，然后通过start_bidx_of_node函数获取到当前block是属于从inode page开始的第几个block，其实本质上就是start_bidx + ofs_in_node = page->index的值。然后根据page->index找到对应的data page，读入到内存中以便后续使用。最后就是将该inode加入到上面提及过的inode list中。

第三阶段(phase=3): 从inode list中取出一个inode，然后根据start_bidx + ofs_in_node找到对应的page->index，然后通过move_data_page函数，将数据写入到其他segment中。




# 7/15 
了解了f2fs_summary的作用以及冷热分离的识别机制。分析工作集算法扩展至冷热分离中可能存在的问题。


# 7/17 7/18
通过对f2fs源代码的分析，完成对文件数据的存储以及读写部分代码的解读注释。

F2FS的读流程包含了以下几个子流程:
vfs_read函数
generic_file_read_iter函数: 根据访问类型执行不同的处理
generic_file_buffered_read: 根据用户传入的文件偏移，读取尺寸等信息，计算起始位置和页数，然后遍历每一个page，通过预读或者单个读取的方式从磁盘中读取出来
f2fs_read_data_page&f2fs_read_data_pages函数: 从磁盘读取1个page或者多个page
f2fs_mpage_readpages函数: f2fs读取数据的主流程

F2FS的写流程主要包含了以下几个子流程:
调用vfs_write函数
调用f2fs_file_write_iter函数: 初始化f2fs_node的信息
调用f2fs_write_begin函数: 创建page cache，并填充数据
写入到page cache: 等待系统触发writeback回写到磁盘
调用f2fs_write_end函数: 将page设置为最新状态
调用f2fs_write_data_pages函数: 系统writeback或者fsync触发的时候执行这个函数写入到磁盘

并分析了改变冷热分离策略可能带来的依赖影响。


# 7/20
F2FS工具的准备
主要使用到用于格式化的mkfs.f2fs工具，ubuntu下可以执行以下命令进行安装
sudo apt-get install f2fs-tools
编译可运行F2FS的内核
经过上述步骤的编译，会在Linux的根目录生成一个.config文件，打开这个文件，找到以下的内核选项，并设置为y。
CONFIG_F2FS_FS=y
CONFIG_F2FS_STAT_FS=y
CONFIG_F2FS_FS_XATTR=y
CONFIG_F2FS_FS_POSIX_ACL=y
然后重新编译
$ make bzImage –j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-
$ make dtbs
编译结束后，创建一个文件作为F2FS的磁盘空间
dd if=/dev/zero of=a9rootfs.f2fs bs=1M count=250 # 创建250MB的F2FS空间
mkfs.f2fs a9rootfs.f2fs #使用F2FS格式化工具进行格式化
接下来，通过执行如下命令启动Qemu虚拟机，需要使用-sd选项将刚刚创建的作为F2FS磁盘空间的文件挂载到系统中:
qemu-system-arm  \
        -M vexpress-a9 \
        -m 512M \
        -kernel /home/xxx/kernels/linux4/linux-4.18/arch/arm/boot/zImage \
        -dtb /home/xxx/kernels/linux4/linux-4.18/arch/arm/boot/dts/vexpress-v2p-ca9.dtb \
        -nographic \
        -append "rdinit=/linuxrc console=ttyAMA0 loglevel=8" \
        -sd a9rootfs.f2fs
最后，Qemu完成启动之后，在Qemu的linux系统执行如下命令将F2FS挂载到linux中:
mount -t f2fs /dev/mmcblk0 /mnt/ -o loop
然后就可以在/mnt目录下通过F2FS对文件进行操作和测试。

# 7/21
F2FS有两种选择segment进行计算cost的算法，分别是Greedy算法和Cost-Benefit算法。
static inline unsigned int get_gc_cost(struct f2fs_sb_info *sbi,
			unsigned int segno, struct victim_sel_policy *p)
{
	if (p->alloc_mode == SSR)
		return get_seg_entry(sbi, segno)->ckpt_valid_blocks;

	/* alloc_mode == LFS */
	if (p->gc_mode == GC_GREEDY)
		return get_valid_blocks(sbi, segno, sbi->segs_per_sec); // Greedy算法，valid block越多表示cost越大，越不值得gc
	else
		return get_cb_cost(sbi, segno); // Cost-Benefit算法，这个是考虑了访问时间和valid block开销的算法
}

我们可以将Cost-Benefit算法理解为一个平衡invalid block数目以及修改时间的的一个算法，在f2fs的实现如下:
static unsigned int get_cb_cost(struct f2fs_sb_info *sbi, unsigned int segno)
{
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int secno = GET_SECNO(sbi, segno);
	unsigned int start = secno * sbi->segs_per_sec;
	unsigned long long mtime = 0;
	unsigned int vblocks;
	unsigned char age = 0;
	unsigned char u;
	unsigned int i;

	for (i = 0; i < sbi->segs_per_sec; i++)
		mtime += get_seg_entry(sbi, start + i)->mtime; // 计算section里面的每一个segment最近一次访问时间
	vblocks = get_valid_blocks(sbi, segno, sbi->segs_per_sec); // 获取当前的section有多少个valid block

	mtime = div_u64(mtime, sbi->segs_per_sec); // 计算平均每一segment的最近一次访问时间
	vblocks = div_u64(vblocks, sbi->segs_per_sec); // 计算平均每一个segment的valid block个数

	u = (vblocks * 100) >> sbi->log_blocks_per_seg; // 百分比计算所以乘以100，然后计算得到了valid block的比例

	/* sit_i->min_mtime以及sit_i->max_mtime计算的是每一次gc的时候的最小最大的修改时间，因此通过这个比例计算这个section的修改时间在总体的情况下的表现 */
	if (mtime < sit_i->min_mtime)
		sit_i->min_mtime = mtime;
	if (mtime > sit_i->max_mtime)
		sit_i->max_mtime = mtime;
	if (sit_i->max_mtime != sit_i->min_mtime)
		age = 100 - div64_u64(100 * (mtime - sit_i->min_mtime),
				sit_i->max_mtime - sit_i->min_mtime);

	return UINT_MAX - ((100 * (100 - u) * age) / (100 + u));
}
在计算平均每一segment的最近一次访问时间和计算平均每一个segment的valid block个数的基础上，我们根据每一个segment中block的平均访问次数。
