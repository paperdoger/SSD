/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: HoitFsLib.c
**
** 创   建   人: Hoit Group
**
** 文件创建日期: 2021 年 03 月 20 日
**
** 描        述: Hoit文件系统内部函数.
*********************************************************************************************************/

#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
#include "../SylixOS/fs/include/fs_fs.h"
#include "../../driver/mtd/nor/nor.h"
/*********************************************************************************************************
  裁剪宏
*********************************************************************************************************/
#if LW_CFG_MAX_VOLUMES > 0
#include "hoitFsLib.h"

/*********************************************************************************************************
** 函数名称: __hoit_just_open
** 功能描述: 打开某个已打开的目录文件下面的一个文件
**           注意pcName是该目录文件下的一个文件名(相对路径)，要打开的文件必须是目录文件pdir的直接子文件，否则返回NULL
** 输　入  :
** 输　出  : 打开结果
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PHOIT_INODE_INFO  __hoit_just_open(PHOIT_INODE_INFO  pdir,
    CPCHAR       pcName)
{

    if (pdir == LW_NULL || !S_ISDIR(pdir->HOITN_mode)) {
        printk("Error in hoit_just_open\n");
        return (LW_NULL);
    }
    UINT newHash = __hoit_name_hash(pcName);
    PLW_LIST_LINE plineTemp;
    PHOIT_FULL_DIRENT pfile = pdir->HOITN_dents;
    while (pfile != LW_NULL) {
        if (pfile->HOITFD_nhash == newHash && lib_strcmp(pfile->HOITFD_name, pcName) == 0) {
            return __hoit_get_full_file(pdir->HOITN_volume, pfile->HOITFD_ino);
        }
        else {
            plineTemp = _list_line_get_next(pfile->HOITFD_next);
            pfile = _LIST_ENTRY(plineTemp, HOIT_FULL_DIRENT, HOITFD_next);
        }
    }

    return  (LW_NULL);                                                  /*  无法找到节点                */
}

/*********************************************************************************************************
** 函数名称: __hoit_name_hash
** 功能描述: 根据文件名计算出其hash值
** 输　入  :
** 输　出  :
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT __hoit_name_hash(PCHAR pname) {
    UINT ret = 0;
    while (*pname != PX_EOS) {
        ret += *pname;
    }
    return ret;
}
/*********************************************************************************************************
** 函数名称: __hoit_free_full_dirent
** 功能描述: 释放FullDirent及其文件名
** 输　入  :
** 输　出  :
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT __hoit_free_full_dirent(PHOIT_FULL_DIRENT pDirent) {
    __SHEAP_FREE(pDirent->HOITFD_file_name);
    __SHEAP_FREE(pDirent);
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_get_full_file
** 功能描述: 根据inode number，创建相应full_xxx结构体（目录文件创建出链表，普通文件创建出红黑树）
** 输　入  :
** 输　出  :
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PHOIT_INODE_INFO __hoit_get_full_file(PHOIT_VOLUME pfs, UINT ino) {
    if (pfs == LW_NULL) {
        printk("Error in hoit_get_full_file\n");
    }
    PLW_LIST_LINE plineTemp;
    PHOIT_INODE_CACHE pcache = pfs->HOITFS_cache_list;

    while (pcache && pcache->HOITC_ino != ino) {
        plineTemp = _list_line_get_next(pcache->HOITC_next);
        pcache = _LIST_ENTRY(plineTemp, HOIT_INODE_CACHE, HOITC_next);
    }
    //************************************ TODO ************************************


    //************************************ END  ************************************
    return LW_NULL;
}

/*********************************************************************************************************
** 函数名称: __hoit_get_inode_cache
** 功能描述: 根据inode number，返回inode_cache，没有就返回NULL
** 输　入  :
** 输　出  :
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PHOIT_INODE_CACHE __hoit_get_inode_cache(PHOIT_VOLUME pfs, UINT ino) {
    if (pfs == LW_NULL) {
        printk("Error in hoit_get_full_file\n");
    }
    PLW_LIST_LINE plineTemp;
    PHOIT_INODE_CACHE pcache = pfs->HOITFS_cache_list;

    while (pcache && pcache->HOITC_ino != ino) {
        plineTemp = _list_line_get_next(pcache->HOITC_next);
        pcache = _LIST_ENTRY(plineTemp, HOIT_INODE_CACHE, HOITC_next);
    }
    
    return pcache;
}

/*********************************************************************************************************
** 函数名称: __hoit_add_dirent
** 功能描述: 给目录文件中添加一个dirent（涉及nhash）
** 输　入  :
** 输　出  :
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID  __hoit_add_dirent(PHOIT_INODE_INFO  pFatherInode,
    PHOIT_FULL_DIRENT pSonDirent)
{

    PHOIT_RAW_DIRENT    pRawDirent = (PHOIT_RAW_DIRENT)__SHEAP_ALLOC(sizeof(HOIT_RAW_DIRENT));
    PHOIT_RAW_INFO      pRawInfo = (PHOIT_RAW_INFO)__SHEAP_ALLOC(sizeof(HOIT_RAW_INFO));
    if (pRawDirent == LW_NULL || pRawInfo == LW_NULL || pFatherInode == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (LW_NULL);
    }
    PHOIT_VOLUME pfs = pFatherInode->HOITN_volume;
    lib_bzero(pRawDirent, sizeof(HOIT_RAW_DIRENT));
    lib_bzero(pRawInfo, sizeof(HOIT_RAW_INFO));

    pRawDirent->file_type = pSonDirent->HOITFD_file_type;
    pRawDirent->ino = pSonDirent->HOITFD_ino;
    pRawDirent->magic_num = HOIT_MAGIC_NUM;
    pRawDirent->pino = pSonDirent->HOITFD_pino;
    pRawDirent->totlen = sizeof(HOIT_RAW_DIRENT) + lib_strlen(pSonDirent->HOITFD_file_name);

    pRawInfo->phys_addr = pfs->HOITFS_now_block->HOITB_offset + pfs->HOITFS_now_block->HOITB_addr;
    pRawInfo->totlen = pRawDirent->totlen;

    __hoit_write_flash(pfs, (PVOID)pRawDirent, sizeof(HOIT_RAW_DIRENT), LW_NULL);
    __hoit_write_flash(pfs, (PVOID)(pSonDirent->HOITFD_file_name), lib_strlen(pSonDirent->HOITFD_file_name, LW_NULL);

    PHOIT_INODE_CACHE pInodeCache = __hoit_get_inode_cache(pfs, pFatherInode->HOITN_ino);
    pRawInfo->next_phys = pInodeCache->HOITC_nodes;
    pInodeCache->HOITC_nodes = pRawInfo;
    pSonDirent->HOITFD_raw_info = pRawInfo;
    __hoit_add_to_dents(pFatherInode, pSonDirent);
    __SHEAP_FREE(pRawDirent);
}
/*********************************************************************************************************
** 函数名称: __hoit_alloc_ino
** 功能描述: 向文件系统申请一个新的inode number
** 输　入  :
** 输　出  :
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT __hoit_alloc_ino(PHOIT_VOLUME pfs) {
    if (pfs == LW_NULL) {
        printk("Error in hoit_get_full_file\n");
    }
    return pfs->HOITFS_highest_ino++;
}
/*********************************************************************************************************
** 函数名称: __hoit_write_flash
** 功能描述: 写入物理设备，不能自己选物理地址
** 输　入  :
** 输　出  : <0代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_write_flash(PHOIT_VOLUME pfs, PVOID pdata, UINT length, UINT* phys_addr) {
    write_nor(pfs->HOITFS_now_block->HOITB_offset + pfs->HOITFS_now_block->HOITB_addr, (PCHAR)(pdata), length, WRITE_KEEP);
    pfs->HOITFS_now_block->HOITB_offset += length;
    if (phys_addr != LW_NULL) {
        *phys_addr = pfs->HOITFS_now_block->HOITB_offset + pfs->HOITFS_now_block->HOITB_addr;
    }
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_write_flash_thru
** 功能描述: 写入物理设备，可以自己选物理地址
** 输　入  :
** 输　出  : <0代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_write_flash_thru(PHOIT_VOLUME pfs, PVOID pdata, UINT length, UINT phys_addr) {
    write_nor(phys_addr, (PCHAR)(pdata), length, WRITE_KEEP);
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_add_to_inode_cache
** 功能描述: 将一个raw_info加入到inode_cache中
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_add_to_inode_cache(PHOIT_INODE_CACHE pInodeCache, PHOIT_RAW_INFO pRawInfo) {
    if (pInodeCache == LW_NULL || pRawInfo == LW_NULL) {
        printk("Error in hoit_add_to_inode_cache\n");
        return HOIT_ERROR;
    }
    pRawInfo->next_phys = pInodeCache->HOITC_nodes;
    pInodeCache->HOITC_nodes = pRawInfo;
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_add_to_cache_list
** 功能描述: 将一个raw_info加入到inode_cache中
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_add_to_cache_list(PHOIT_VOLUME pfs, PHOIT_INODE_CACHE pInodeCache) {
    if (pfs == LW_NULL || pInodeCache == LW_NULL) {
        printk("Error in hoit_add_to_inode_cache\n");
        return HOIT_ERROR;
    }
    
    pInodeCache->HOITC_next = pfs->HOITFS_cache_list;
    pfs->HOITFS_cache_list = pInodeCache->HOITC_next;
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_add_to_dents
** 功能描述: 将一个full_dirent加入到父目录文件夹的dents中
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_add_to_dents(PHOIT_INODE_INFO pInodeFather, PHOIT_FULL_DIRENT pFullDirent) {
    if (pInodeFather == LW_NULL || pFullDirent == LW_NULL) {
        printk("Error in hoit_add_to_dents\n");
        return HOIT_ERROR;
    }

    pFullDirent->HOITFD_next = pInodeFather->HOITN_dents;
    pInodeFather->HOITN_dents = pFullDirent;
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_search_in_dents
** 功能描述: 在一个父dents链表中用二分法搜索一个指定ino的文件，返回FullDirent
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PHOIT_FULL_DIRENT __hoit_search_in_dents(PHOIT_INODE_INFO pInodeFather, UINT ino) {
    if (pInodeFather == LW_NULL) {
        printk("Error in hoit_search_in_dents\n");
        return HOIT_ERROR;
    }
    PHOIT_FULL_DIRENT pDirent = pInodeFather->HOITN_dents;
    while (pDirent && pDirent->HOITFD_ino != ino) {
        pDirent = pDirent->HOITFD_next;
    }
    if (pDirent) 
        return pDirent;
    return LW_NULL;
}
/*********************************************************************************************************
** 函数名称: __hoit_del_raw_info
** 功能描述: 将一个RawInfo从对应的InodeCache链表中删除，但不free对应内存空间
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_del_raw_info(PHOIT_INODE_CACHE pInodeCache, PHOIT_RAW_INFO pRawInfo) {
    if (pInodeCache == LW_NULL || pRawInfo == LW_NULL) {
        printk("Error in hoit_add_to_dents\n");
        return HOIT_ERROR;
    }
    if (pInodeCache->HOITC_nodes == pRawInfo) {
        pInodeCache->HOITC_nodes = pRawInfo->next_phys;
        return 0;
    }
    else {
        PHOIT_RAW_INFO pRawTemp = pInodeCache->HOITC_nodes;
        while (pRawTemp && pRawTemp->next_phys != pRawInfo) {
            pRawTemp = pRawTemp->next_phys;
        }
        if (pRawTemp) {
            pRawTemp->next_phys = pRawInfo->next_phys;
            return 0;
        }
        else {
            return HOIT_ERROR;
        }
    }
    
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_del_raw_data
** 功能描述: 将一个RawDirent或RawInode在对应的磁盘中标记为过期,并不释放RawInfo内存
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_del_raw_data(PHOIT_RAW_INFO pRawInfo) {
    if (pRawInfo == LW_NULL) {
        printk("Error in hoit_del_raw_data\n");
        return HOIT_ERROR;
    }

    PCHAR buf = (PCHAR)__SHEAP_ALLOC(pRawInfo->totlen);
    lib_bzero(buf, pRawInfo->totlen);
    read_nor(pRawInfo->phys_addr, buf, pRawInfo->totlen);

    PHOIT_RAW_HEADER pRawHeader = (PHOIT_RAW_HEADER)buf;
    if (pRawHeader->magic_num != HOIT_MAGIC_NUM || pRawDirent->flag & HOIT_FLAG_OBSOLETE == 0) {
        printk("Error in hoit_del_raw_data\n");
        return HOIT_ERROR;
    }
    pRawHeader->flag &= (~HOIT_FLAG_OBSOLETE);      //将obsolete标志变为0，代表过期
    
    __hoit_write_flash_thru(LW_NULL, (PVOID)pRawHeader, pRawInfo->totlen, pRawInfo->phys_addr);
    __SHEAP_FREE(buf);
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_del_full_dirent
** 功能描述: 将一个FullDirent从对应的InodeInfo的dents链表中删除，但不free对应内存空间
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_del_full_dirent(PHOIT_INODE_INFO pInodeInfo, PHOIT_FULL_DIRENT pFullDirent) {
    if (pInodeInfo == LW_NULL || pFullDirent == LW_NULL) {
        printk("Error in hoit_del_full_dirent\n");
        return HOIT_ERROR;
    }
    if (pInodeInfo->HOITN_dents == pFullDirent) {
        pInodeInfo->HOITN_dents = pFullDirent->HOITFD_next;
        return 0;
    }
    else {
        PHOIT_FULL_DIRENT pFullTemp = pInodeInfo->HOITN_dents;
        while (pFullTemp && pFullTemp->HOITFD_next != pFullDirent) {
            pFullTemp = pFullTemp->HOITFD_next;
        }
        if (pFullTemp) {
            pFullTemp->HOITFD_next = pFullDirent->HOITFD_next;
            return 0;
        }
        else {
            return HOIT_ERROR;
        }
    }
    return 0;
}
/*********************************************************************************************************
** 函数名称: __hoit_del_inode_cache
** 功能描述: 将一个InodeCache从挂载的文件系统中删除，但不free对应内存空间
** 输　入  :
** 输　出  : !=0 代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
UINT8 __hoit_del_inode_cache(PHOIT_VOLUME pfs, PHOIT_INODE_CACHE pInodeCache) {
    PHOIT_INODE_CACHE pTemp = pfs->HOITFS_cache_list;
    if (pTemp == pInodeCache) {
        pfs->HOITFS_cache_list = pInodeCache->HOITC_next;
        return 0;
    }
    while (pTemp && pTemp->HOITC_next != pInodeCache) {
        pTemp = pTemp->HOITC_next;
    }
    if (pTemp == LW_NULL) return HOIT_ERROR;
    pTemp->HOITC_next = pInodeCache->HOITC_next;
    return 0;
}


/*********************************************************************************************************
** 函数名称: __hoit_open
** 功能描述: hoitfs 打开一个文件
** 输　入  : pfs              文件系统
**           pcName           文件名
**           ppinodeFather     当无法找到节点时保存最接近的一个,
                              但寻找到节点时保存父系节点.
                              LW_NULL 表示根
             pbRoot           是否为根节点
**           pbLast           当匹配失败时, 是否是最后一级文件匹配失败
**           ppcTail          如果存在连接文件, 指向连接文件后的路径
** 输　出  : 打开结果
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PHOIT_INODE_INFO  __hoit_open(PHOIT_VOLUME  pfs,
    CPCHAR       pcName,
    PHOIT_INODE_INFO* ppinodeFather,
    BOOL* pbRoot,
    BOOL* pbLast,
    PCHAR* ppcTail)
{
    CHAR                pcTempName[MAX_FILENAME_LENGTH];
    PCHAR               pcNext;
    PCHAR               pcNode;

    PHOIT_INODE_INFO    pinodeTemp;

    if (ppinodeFather == LW_NULL) {
        ppinodeFather = &pinodeTemp;                                      /*  临时变量                    */
    }
    *ppinodeFather = LW_NULL;
    UINT inodeFatherIno = 0;
    if (*pcName == PX_ROOT) {                                           /*  忽略根符号                  */
        lib_strlcpy(pcTempName, (pcName + 1), PATH_MAX);
    }
    else {
        lib_strlcpy(pcTempName, pcName, PATH_MAX);
    }

    if (pcTempName[0] == PX_EOS) {
        if (pbRoot) {
            *pbRoot = LW_TRUE;                                          /*  pcName 为根                 */
        }
        if (pbLast) {
            *pbLast = LW_FALSE;
        }
        return  (LW_NULL);
    }
    else {
        if (pbRoot) {
            *pbRoot = LW_FALSE;                                         /*  pcName 不为根               */
        }
    }
    PHOIT_INODE_INFO    pInode;
    PHOIT_FULL_DIRENT   pDirentTemp;

    pcNext = pcTempName;
    pInode = pfs->HOITFS_pRootDir;                               /*  从根目录开始搜索            */

    do {
        pcNode = pcNext;
        pcNext = lib_index(pcNode, PX_DIVIDER);                         /*  移动到下级目录              */
        if (pcNext) {                                                   /*  是否可以进入下一层          */
            *pcNext = PX_EOS;
            pcNext++;                                                   /*  下一层的指针                */
        }

        for (pDirentTemp = pInode->HOITN_dents;
            pDirentTemp != LW_NULL;
            pDirentTemp = pDirentTemp->HOITFD_next) {

            if (pDirentTemp == LW_NULL) {                                     /*  无法继续搜索                */
                goto    __find_error;
            }
            if (S_ISLNK(pDirentTemp->HOITFD_file_type)) {                            /*  链接文件                    */
                if (lib_strcmp(pDirentTemp->HOITFD_file_name, pcNode) == 0) {
                    goto    __find_ok;                                  /*  找到链接                    */
                }

            }
            else if (S_ISDIR(pDirentTemp->HOITFD_file_type)) {
                if (lib_strcmp(pDirentTemp->HOITFD_file_name, pcNode) == 0) {      /*  已经找到一级目录            */
                    break;
                }
            }
            else {
                if (lib_strcmp(pDirentTemp->HOITFD_file_name, pcNode) == 0) {
                    if (pcNext) {                                       /*  还存在下级, 这里必须为目录  */
                        goto    __find_error;                           /*  不是目录直接错误            */
                    }
                    break;
                }
            }
        }

        inodeFatherIno = pDirentTemp->HOITFD_ino;                       /*  从当前节点开始搜索          */
        pInode = __hoit_get_full_file(pfs, inodeFatherIno);             /*  从第一个儿子开始            */
    } while (pcNext);                                                   /*  不存在下级目录              */

__find_ok:
    *ppinodeFather = __hoit_get_full_file(pfs, pDirentTemp->HOITFD_pino);                            /*  父系节点                    */
    /*
     *  计算 tail 的位置.
     */
    if (ppcTail) {
        if (pcNext) {
            INT   iTail = pcNext - pcTempName;
            *ppcTail = (PCHAR)pcName + iTail;                           /*  指向没有被处理的 / 字符     */
        }
        else {
            *ppcTail = (PCHAR)pcName + lib_strlen(pcName);              /*  指向最末尾                  */
        }
    }
    return  (pInode);

__find_error:
    if (pbLast) {
        if (pcNext == LW_NULL) {                                        /*  最后一级查找失败            */
            *pbLast = LW_TRUE;
        }
        else {
            *pbLast = LW_FALSE;
        }
    }
    return  (LW_NULL);                                                  /*  无法找到节点                */
}


/*********************************************************************************************************
** 函数名称: __hoit_maken
** 功能描述: HoitFs 创建一个文件
** 输　入  : pfs              文件系统
**           pcName           文件名，不能含有上级目录的名称
**           pInodeFather     父亲, NULL 表示根目录
**           mode             mode_t
**           pcLink           如果为连接文件, 这里指明连接目标.
** 输　出  : 创建结果
** 全局变量:
** 调用模块:
*********************************************************************************************************/
PHOIT_INODE_INFO  __hoit_maken(PHOIT_VOLUME  pfs,
    CPCHAR       pcName,
    PHOIT_INODE_INFO    pInodeFather,
    mode_t       mode,
    CPCHAR       pcLink)
{
    PHOIT_RAW_INODE     pRawInode   = (PHOIT_RAW_INODE)__SHEAP_ALLOC(sizeof(HOIT_RAW_INODE));
    PHOIT_RAW_INFO     pRawInfo = (PHOIT_RAW_INFO)__SHEAP_ALLOC(sizeof(HOIT_RAW_INFO));
    PHOIT_INODE_CACHE   pInodeCache = (PHOIT_INODE_CACHE)__SHEAP_ALLOC(sizeof(HOIT_INODE_CACHE));
    PHOIT_FULL_DIRENT   pFullDirent = (PHOIT_FULL_DIRENT)__SHEAP_ALLOC(sizeof(HOIT_FULL_DIRENT));
    PCHAR      pcFileName;

    if (pRawInfo == LW_NULL || pRawInode == LW_NULL || pInodeCache == LW_NULL || pFullDirent == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (LW_NULL);
    }

    lib_bzero(pRawInode, sizeof(HOIT_RAW_INODE));
    lib_bzero(pRawInfo, sizeof(HOIT_RAW_INFO));
    lib_bzero(pInodeCache, sizeof(HOIT_INODE_CACHE));
    lib_bzero(pFullDirent, sizeof(HOIT_FULL_DIRENT));

    pcFileName = lib_rindex(pcName, PX_DIVIDER);
    if (pcFileName) {
        pcFileName++;
    }
    else {
        pcFileName = pcName;
    }

    pRawInode->file_type = mode;
    pRawInode->ino = __hoit_alloc_ino(pfs);
    pRawInode->magic_num = HOIT_MAGIC_NUM;
    pRawInode->totlen = sizeof(HOIT_RAW_INODE);
    pRawInode->flag = HOIT_FLAG_TYPE_INODE | HOIT_FLAG_OBSOLETE;

    UINT phys_addr;
    __hoit_write_flash(pfs, (PVOID)pRawInode, sizeof(HOIT_RAW_INODE), &phys_addr);
   
    pRawInfo->phys_addr = phys_addr;
    pRawInfo->totlen = sizeof(HOIT_RAW_INODE);

    pInodeCache->HOITC_ino = pRawInode->ino;

    __hoit_add_to_inode_cache(pInodeCache, pRawInfo);
    __hoit_add_to_cache_list(pfs, pInodeCache);


    pFullDirent->HOITFD_file_name = (PCHAR)__SHEAP_ALLOC(lib_strlen(pcFileName) + 1);
    if (pFullDirent->HOITFD_file_name == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (LW_NULL);
    }
    lib_strcpy(pFullDirent->HOITFD_file_name, pcFileName);
    pFullDirent->HOITFD_file_type = mode;
    pFullDirent->HOITFD_ino = pRawInode->ino;
    pFullDirent->HOITFD_nhash = __hoit_name_hash(pcFileName);
    pFullDirent->HOITFD_pino = pInodeFather->ino;

    __hoit_add_dirent(pInodeFather, pFullDirent);

    __SHEAP_FREE(pRawInode);
    /*
    *   已经将新文件配置成了一个已经存在的文件，现在只需调用get_full_file即可
    */
    return  __hoit_get_full_file(pfs, pFullDirent->HOITFD_ino);
}
/*********************************************************************************************************
** 函数名称: __hoit_unlink_regular
** 功能描述: HoitFs 将普通文件链接数减1，将相应的FullDirent标记为过期，如果链接数减至为0则文件的RawInode也将被标记过期
**           相当于本函数只删除非目录文件
**           注意参数传进来的pDirent不会在该函数内被释放，应该由调用该函数的上级函数负责释放
** 输　入  : pramn            文件节点
** 输　出  : 删除结果
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __hoit_unlink_regular(PHOIT_INODE_INFO pInodeFather, PHOIT_FULL_DIRENT  pDirent)
{
    if (pDirent == LW_NULL || S_ISDIR(pDirent->HOITFD_file_type)) {
        _ErrorHandle(ENOTEMPTY);
        return  (PX_ERROR);
    }
    PHOIT_VOLUME pfs = pInodeFather->HOITN_volume;
    PHOIT_INODE_CACHE pInodeCache = __hoit_get_inode_cache(pfs, pDirent->HOITFD_ino);
    pInodeCache->HOITC_nlink -= 1;

    PHOIT_INODE_CACHE pFatherInodeCache = __hoit_get_inode_cache(pfs, pInodeFather->HOITN_ino);
    /*
    *将被删除的FullDirent对应的RawInfo和Flash上的RawDirent删除
    */
    PHOIT_RAW_INFO pRawInfo = pDirent->HOITFD_raw_info;
    __hoit_del_raw_info(pFatherInodeCache, pRawInfo);     //将RawInfo从InodeCache的链表中删除
    __hoit_del_raw_data(pRawInfo);
    __SHEAP_FREE(pRawInfo);
    /*
    *将该FullDirent从父目录文件中的dents链表删除
    */
    __hoit_del_full_dirent(pInodeFather, pDirent);

    /*
    *如果nlink减为0，则将该InodeCache对应的文件所有在Flash上的数据标记为过期并释放掉内存中的InodeCache
    */
    if (pInodeCache->HOITC_nlink == 0) {
        PHOIT_RAW_INFO pRawTemp = pInodeCache->HOITC_nodes;
        PHOIT_RAW_INFO pRawNext = LW_NULL;
        while (pRawTemp) {
            __hoit_del_raw_data(pRawTemp);
            pRawNext = pRawTemp->next_phys;
            __SHEAP_FREE(pRawTemp);
            pRawTemp = pRawNext;
        }
    }
    __hoit_del_inode_cache(pfs, pInodeCache);
    __SHEAP_FREE(pInodeCache);
    return  (ERROR_NONE);
}

/*********************************************************************************************************
** 函数名称: __hoit_truncate
** 功能描述: hoitfs 截断一个文件
** 输　入  : pInodeInfo       文件节点
**           offset            截断点
** 输　出  : 缩短结果
** 全局变量:
** 调用模块:
*********************************************************************************************************/
VOID  __hoit_truncate(PHOIT_INODE_INFO  pInodeInfo, size_t  offset)
{
    //************************************ TODO ************************************


    //************************************ END  ************************************
}

/*********************************************************************************************************
** 函数名称: __hoit_unlink_dir
** 功能描述: 将一个目录文件删除，包括对其所有子文件进行删除（普通文件调用__hoit_unlink_regular，如果有子文件是目录文件则递归调用__hoit_unlink_dir）
**           相当于本函数只删除目录文件（与ramfs不同，本函数既删除目录文件下的子文件，又删除目录文件本身）
**           注意参数传进来的pDirent不会在该函数内被释放，应该由调用该函数的上级函数负责释放
** 输　入  : pramn            文件节点
** 输　出  : !=0代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __hoit_unlink_dir(PHOIT_INODE_INFO pInodeFather, PHOIT_FULL_DIRENT  pDirent) {
    if (pDirent == LW_NULL || !S_ISDIR(pDirent->HOITFD_file_type)) {
        _ErrorHandle(ENOTEMPTY);
        return  (PX_ERROR);
    }
    PHOIT_VOLUME pfs = pInodeFather->HOITN_volume;
    PHOIT_INODE_CACHE pInodeCache = __hoit_get_inode_cache(pfs, pDirent->HOITFD_ino);
    

    PHOIT_INODE_CACHE pFatherInodeCache = __hoit_get_inode_cache(pfs, pInodeFather->HOITN_ino);
    /*
    *将被删除的FullDirent对应的RawInfo和Flash上的RawDirent删除
    */
    PHOIT_RAW_INFO pRawInfo = pDirent->HOITFD_raw_info;
    __hoit_del_raw_info(pFatherInodeCache, pRawInfo);     //将RawInfo从InodeCache的链表中删除
    __hoit_del_raw_data(pRawInfo);
    __SHEAP_FREE(pRawInfo);
    /*
    *将该FullDirent从父目录文件中的dents链表删除，接着将FullDirent内存释放掉
    */
    __hoit_del_full_dirent(pInodeFather, pDirent);
;
    /*
    *目录文件nlink为1，再减1就变为0了，必须先尝试unlink子文件，再删除目录文件本身的数据
    */
    if (pInodeCache->HOITC_nlink == 1) {
        //先打开目录文件
        PHOIT_INODE_INFO pDirFileInode = __hoit_get_full_file(pfs, pInodeCache->HOITC_ino);
        if (!S_ISDIR(pDirFileInode->HOITN_mode)) return HOIT_ERROR;

        //再一次unlink目录文件下的每个子文件
        PHOIT_FULL_DIRENT pFullDirent = pDirFileInode->HOITN_dents;
        PHOIT_FULL_DIRENT pFullDirentNext = LW_NULL;
        while (pFullDirent) {
            pFullDirentNext = pFullDirent->HOITFD_next;
            if (!S_ISDIR(pFullDirent->HOITFD_file_type)) {
                __hoit_unlink_regular(pDirFileInode, pFullDirent);
            }
            else {
                __hoit_unlink_dir(pDirFileInode, pFullDirent);
            }
            __hoit_free_full_dirent(pFullDirent);
            pFullDirent = pFullDirentNext;
        }

        //每个目录文件有一个自己的RawInode需要我们自己删除
        PHOIT_RAW_INFO pRawTemp = pInodeCache->HOITC_nodes;
        PHOIT_RAW_INFO pRawNext = LW_NULL;
        while (pRawTemp) {
            __hoit_del_raw_data(pRawTemp);
            pRawNext = pRawTemp->next_phys;
            __SHEAP_FREE(pRawTemp);
            pRawTemp = pRawNext;
        }

        __hoit_del_inode_cache(pfs, pInodeCache);
        __SHEAP_FREE(pDirFileInode);
        __SHEAP_FREE(pInodeCache);
    }
    pInodeCache->HOITC_nlink -= 1;
    return ERROR_NONE;
}
/*********************************************************************************************************
** 函数名称: __hoit_move_check
** 功能描述: HoitFs 检查第二个节点是否为第一个节点的子孙
** 输　入  : pInode1       第一个节点
**           pInode2       第二个节点
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __hoit_move_check(PHOIT_INODE_INFO  pInode1, PHOIT_INODE_INFO  pInode2)
{
    PHOIT_VOLUME pfs = pInode1->HOITN_volume;
    PHOIT_INODE_CACHE pCache1 = pInode1->HOITN_inode_cache;
    PHOIT_INODE_CACHE pCache2 = pInode2->HOITN_inode_cache;
    do {
        if (pCache1->HOITC_ino == pCache2->HOITC_ino) {
            return  (PX_ERROR);
        }
        pCache2 = __hoit_get_inode_cache(pfs, pCache2->HOITC_ino);
    } while (pCache2);

    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __hoit_move
** 功能描述: HoitFs 移动或者重命名一个文件
** 输　入  : pInodeFather     文件的父目录节点（move之前的）
**           pInodeInfo       文件节点
**           pcNewName        新的名字
** 输　出  : ERROR
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __hoit_move(PHOIT_INODE_INFO pInodeFather, PHOIT_INODE_INFO  pInodeInfo, PCHAR  pcNewName)
{
    INT         iRet;
    PHOIT_VOLUME pfs;
    PHOIT_INODE_INFO   pInodeTemp;
    PHOIT_INODE_INFO   pInodeNewFather;
    BOOL        bRoot;
    BOOL        bLast;
    PCHAR       pcTail;
    PCHAR       pcTemp;
    PCHAR       pcFileName;

    pfs = pInodeInfo->HOITN_volume;

    pInodeTemp = __hoit_open(pfs, pcNewName, &pInodeNewFather, &bRoot, &bLast, &pcTail);
    if (!pInodeTemp && (bRoot || (bLast == LW_FALSE))) {                 /*  新名字指向根或者没有目录    */
        _ErrorHandle(EINVAL);
        return  (PX_ERROR);
    }

    if (pInodeInfo == pInodeTemp) {                                           /*  相同                        */
        return  (ERROR_NONE);
    }

    if (S_ISDIR(pInodeInfo->HOITN_mode) && pInodeNewFather) {
        if (__hoit_move_check(pInodeInfo, pInodeNewFather)) {                  /*  检查目录合法性              */
            _ErrorHandle(EINVAL);
            return  (PX_ERROR);
        }
    }
    pcFileName = lib_rindex(pcNewName, PX_DIVIDER);
    if (pcFileName) {
        pcFileName++;
    }
    else {
        pcFileName = pcNewName;
    }

    pcTemp = (PCHAR)__SHEAP_ALLOC(lib_strlen(pcFileName) + 1);          /*  预分配名字缓存              */
    if (pcTemp == LW_NULL) {
        _ErrorHandle(ENOMEM);
        return  (PX_ERROR);
    }
    lib_strcpy(pcTemp, pcFileName);

    if (pInodeTemp) {
        if (!S_ISDIR(pInodeInfo->HOITN_mode) && S_ISDIR(pInodeTemp->HOITN_mode)) {
            __SHEAP_FREE(pcTemp);
            _ErrorHandle(EISDIR);
            return  (PX_ERROR);
        }
        if (S_ISDIR(pInodeInfo->HOITN_mode) && !S_ISDIR(pInodeTemp->HOITN_mode)) {
            __SHEAP_FREE(pcTemp);
            _ErrorHandle(ENOTDIR);
            return  (PX_ERROR);
        }

        PHOIT_FULL_DIRENT pFullDirent = __hoit_search_in_dents(pInodeNewFather, pInodeTemp->HOITN_ino);
        if (pFullDirent == LW_NULL) {
            __SHEAP_FREE(pcTemp);
            _ErrorHandle(ENOTDIR);
            return  (PX_ERROR);
        }
        if (S_ISDIR(pInodeTemp->HOITN_mode)) {
            iRet = __hoit_unlink_dir(pInodeNewFather, pFullDirent);     /*  删除目标                    */
        }
        else {
            iRet = __hoit_unlink_regular(pInodeNewFather, pFullDirent); /*  删除目标                    */
        }
        __hoit_free_full_dirent(pFullDirent);
        

        if (iRet) {
            __SHEAP_FREE(pcTemp);
            return  (PX_ERROR);
        }
    }

    if (pInodeFather != pInodeNewFather) {                              /*  目录发生改变                */
        PHOIT_FULL_DIRENT pFullDirent = (PHOIT_FULL_DIRENT)__SHEAP_ALLOC(sizeof(HOIT_FULL_DIRENT));
        pFullDirent->HOITFD_file_name = pcTemp;
        pFullDirent->HOITFD_file_type = pInodeInfo->HOITN_mode;
        pFullDirent->HOITFD_ino = pInodeInfo->HOITN_ino;
        pFullDirent->HOITFD_nhash = __hoit_name_hash(pcTemp);
        pFullDirent->HOITFD_pino = pInodeNewFather->HOITN_ino;
        __hoit_add_dirent(pInodeNewFather, pFullDirent);

        PHOIT_FULL_DIRENT pOldDirent = __hoit_search_in_dents(pInodeFather, pInodeInfo->HOITN_ino);
        __hoit_del_full_dirent(pInodeFather, pOldDirent);
    }
    else {
        PHOIT_FULL_DIRENT pOldDirent = __hoit_search_in_dents(pInodeFather, pInodeInfo->HOITN_ino);
        __hoit_del_full_dirent(pInodeFather, pOldDirent);

        __SHEAP_FREE(pOldDirent->HOITFD_file_name);
        pOldDirent->HOITFD_file_name = pcTemp;

        __hoit_add_to_dents(pInodeFather, pOldDirent);
    }
    return  (ERROR_NONE);
}
/*********************************************************************************************************
** 函数名称: __hoit_stat
** 功能描述: 从一个打开的文件中读取相应数据到stat结构体
** 输　入  : pInodeInfo       文件节点
** 输　出  : !=0代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __hoit_stat(PHOIT_INODE_INFO pInodeInfo, PHOIT_VOLUME  pfs, struct stat* pstat) {
    if (pInodeInfo) {
        pstat->st_dev = LW_DEV_MAKE_STDEV(&pfs->HOITFS_devhdrHdr);
        pstat->st_ino = (ino_t)pInodeInfo;
        pstat->st_mode = pInodeInfo->HOITN_mode;
        pstat->st_nlink = pInodeInfo->HOITN_inode_cache->HOITC_ino;
        pstat->st_uid = pInodeInfo->HOITN_uid;
        pstat->st_gid = pInodeInfo->HOITN_gid;
        pstat->st_rdev = 1;
        pstat->st_size = (off_t)pInodeInfo->HOITN_stSize;
        pstat->st_atime = pInodeInfo->HOITN_timeAccess;
        pstat->st_mtime = pInodeInfo->HOITN_timeChange;
        pstat->st_ctime = pInodeInfo->HOITN_timeCreate;
        pstat->st_blksize = 0;
        pstat->st_blocks = 0;
    }
    else {
        pstat->st_dev = LW_DEV_MAKE_STDEV(&pfs->HOITFS_devhdrHdr);
        pstat->st_ino = (ino_t)0;
        pstat->st_mode = pfs->HOITFS_mode;
        pstat->st_nlink = 1;
        pstat->st_uid = pfs->HOITFS_uid;
        pstat->st_gid = pfs->HOITFS_gid;
        pstat->st_rdev = 1;
        pstat->st_size = 0;
        pstat->st_atime = pfs->HOITFS_time;
        pstat->st_mtime = pfs->HOITFS_time;
        pstat->st_ctime = pfs->HOITFS_time;
        pstat->st_blksize = 0;
        pstat->st_blocks = 0;
    }

    pstat->st_resv1 = LW_NULL;
    pstat->st_resv2 = LW_NULL;
    pstat->st_resv3 = LW_NULL;
}
/*********************************************************************************************************
** 函数名称: __hoit_statfs
** 功能描述: 读取文件系统相关信息到pstatfs
** 输　入  : pInodeInfo       文件节点
** 输　出  : !=0代表出错
** 全局变量:
** 调用模块:
*********************************************************************************************************/
INT  __hoit_statfs(PHOIT_VOLUME  pfs, struct statfs* pstatfs) {
    pstatfs->f_type = TMPFS_MAGIC;  //需要修改
    pstatfs->f_bsize = 0;
    pstatfs->f_blocks = 0;
    pstatfs->f_bfree = 0;
    pstatfs->f_bavail = 1;

    pstatfs->f_files = 0;
    pstatfs->f_ffree = 0;

#if LW_CFG_CPU_WORD_LENGHT == 64
    pstatfs->f_fsid.val[0] = (int32_t)((addr_t)pfs >> 32);
    pstatfs->f_fsid.val[1] = (int32_t)((addr_t)pfs & 0xffffffff);
#else
    pstatfs->f_fsid.val[0] = (int32_t)pfs;
    pstatfs->f_fsid.val[1] = 0;
#endif

    pstatfs->f_flag = 0;
    pstatfs->f_namelen = PATH_MAX;
}
#endif                                                                  /*  LW_CFG_MAX_VOLUMES > 0      */