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
