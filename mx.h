/**
 * @file
 * API for mailboxes
 *
 * @authors
 * Copyright (C) 1996-2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MUTT_MX_H
#define MUTT_MX_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "config/lib.h"
#include "core/lib.h"

struct Email;
struct Context;
struct stat;

extern const struct MxOps *mx_ops[];

/* These Config Variables are only used in mx.c */
extern bool          C_KeepFlagged;
extern unsigned char C_MboxType;
extern unsigned char C_Move;
extern char *        C_Trash;

extern struct EnumDef MboxTypeDef;

/* flags for mutt_open_mailbox() */
typedef uint8_t OpenMailboxFlags;   ///< Flags for mutt_open_mailbox(), e.g. #MUTT_NOSORT
#define MUTT_OPEN_NO_FLAGS       0  ///< No flags are set
#define MUTT_NOSORT        (1 << 0) ///< Do not sort the mailbox after opening it
#define MUTT_APPEND        (1 << 1) ///< Open mailbox for appending messages
#define MUTT_READONLY      (1 << 2) ///< Open in read-only mode
#define MUTT_QUIET         (1 << 3) ///< Do not print any messages
#define MUTT_NEWFOLDER     (1 << 4) ///< Create a new folder - same as #MUTT_APPEND,
                                    ///< but uses mutt_file_fopen() with mode "w" for mbox-style folders.
                                    ///< This will truncate an existing file.
#define MUTT_PEEK          (1 << 5) ///< Revert atime back after taking a look (if applicable)
#define MUTT_APPENDNEW     (1 << 6) ///< Set in mx_open_mailbox_append if the mailbox doesn't exist.
                                    ///< Used by maildir/mh to create the mailbox.

typedef uint8_t MsgOpenFlags;      ///< Flags for mx_msg_open_new(), e.g. #MUTT_ADD_FROM
#define MUTT_MSG_NO_FLAGS       0  ///< No flags are set
#define MUTT_ADD_FROM     (1 << 0) ///< add a From_ line
#define MUTT_SET_DRAFT    (1 << 1) ///< set the message draft flag

/**
 * enum MxCheckReturns - Return values from mx_mbox_check(), mx_mbox_sync(),
 * and mx_mbox_close()
 */
enum MxCheckReturns
{
  MX_CHECK_ERROR = -1, ///< An error occurred
  MX_CHECK_NO_CHANGE,  ///< No changes
  MX_CHECK_NEW_MAIL,   ///< New mail received in Mailbox
  MX_CHECK_LOCKED,     ///< Couldn't lock the Mailbox
  MX_CHECK_REOPENED,   ///< Mailbox was reopened
  MX_CHECK_FLAGS,      ///< Nondestructive flags change (IMAP)
};

/**
 * enum MxCheckStatsReturns - Return values from mx_mbox_check_stats()
 * @note This is a subset of MxCheckReturns.
 */
enum MxCheckStatsReturns
{
  MX_CHECK_STATS_ERROR = -1, ///< An error occurred
  MX_CHECK_STATS_NO_CHANGE,  ///< No changes
  MX_CHECK_STATS_NEW_MAIL,   ///< New mail received in Mailbox
};

/**
 * enum MxOpenReturns - Return values for mbox_open()
 */
enum MxOpenReturns
{
  MX_OPEN_OK,
  MX_OPEN_ERROR,
  MX_OPEN_ABORT
};

/**
 * struct Message - A local copy of an email
 */
struct Message
{
  FILE *fp;             ///< pointer to the message data
  char *path;           ///< path to temp file
  char *committed_path; ///< the final path generated by mx_msg_commit()
  bool write;           ///< nonzero if message is open for writing
  struct
  {
    bool read : 1;
    bool flagged : 1;
    bool replied : 1;
    bool draft : 1;
  } flags;
  time_t received; ///< the time at which this message was received
};

/**
 * struct MxOps - The Mailbox API
 *
 * Each backend provides a set of functions through which the Mailbox, messages,
 * tags and paths are manipulated.
 */
struct MxOps
{
  enum MailboxType type;  ///< Mailbox type, e.g. #MUTT_IMAP
  const char *name;       ///< Mailbox name, e.g. "imap"
  bool is_local;          ///< True, if Mailbox type has local files/dirs

  /**
   * ac_owns_path - Check whether an Account owns a Mailbox path
   * @param a    Account
   * @param path Mailbox Path
   * @retval true  Account handles path
   * @retval false Account does not handle path
   *
   * **Contract**
   * - @a a    is not NULL
   * - @a path is not NULL
   */
  bool (*ac_owns_path) (struct Account *a, const char *path);

  /**
   * ac_add - Add a Mailbox to an Account
   * @param a Account to add to
   * @param m Mailbox to add
   * @retval  true  Success
   * @retval  false Error
   *
   * **Contract**
   * - @a a is not NULL
   * - @a m is not NULL
   */
  bool (*ac_add) (struct Account *a, struct Mailbox *m);

  /**
   * mbox_open - Open a Mailbox
   * @param m Mailbox to open
   * @return enum MxOpenReturns
   *
   * **Contract**
   * - @a m is not NULL
   */
  enum MxOpenReturns (*mbox_open)       (struct Mailbox *m);

  /**
   * mbox_open_append - Open a Mailbox for appending
   * @param m     Mailbox to open
   * @param flags Flags, see #OpenMailboxFlags
   * @retval true Success
   * @retval false Failure
   *
   * **Contract**
   * - @a m is not NULL
   */
  bool (*mbox_open_append)(struct Mailbox *m, OpenMailboxFlags flags);

  /**
   * mbox_check - Check for new mail
   * @param m          Mailbox
   * @return enum MxCheckReturns
   *
   * **Contract**
   * - @a m is not NULL
   */
  enum MxCheckReturns (*mbox_check) (struct Mailbox *m);

  /**
   * mbox_check_stats - Check the Mailbox statistics
   * @param m     Mailbox to check
   * @param flags Function flags
   * @return enum MxCheckStatsReturns
   *
   * **Contract**
   * - @a m is not NULL
   */
  enum MxCheckStatsReturns (*mbox_check_stats)(struct Mailbox *m, int flags);

  /**
   * mbox_sync - Save changes to the Mailbox
   * @param m          Mailbox to sync
   * @return enum MxCheckReturns
   *
   * **Contract**
   * - @a m is not NULL
   */
  enum MxCheckReturns (*mbox_sync)(struct Mailbox *m);

  /**
   * mbox_close - Close a Mailbox
   * @param m Mailbox to close
   * @return enum MxCheckReturns
   *
   * **Contract**
   * - @a m is not NULL
   */
  enum MxCheckReturns (*mbox_close)(struct Mailbox *m);

  /**
   * msg_open - Open an email message in a Mailbox
   * @param m     Mailbox
   * @param msg   Message to open
   * @param msgno Index of message to open
   * @retval  0 Success
   * @retval -1 Error
   *
   * **Contract**
   * - @a m   is not NULL
   * - @a msg is not NULL
   * - 0 <= @a msgno < msg->msg_count
   */
  int (*msg_open)        (struct Mailbox *m, struct Message *msg, int msgno);

  /**
   * msg_open_new - Open a new message in a Mailbox
   * @param m   Mailbox
   * @param msg Message to open
   * @param e   Email
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a m   is not NULL
   * - @a msg is not NULL
   */
  int (*msg_open_new)    (struct Mailbox *m, struct Message *msg, const struct Email *e);

  /**
   * msg_commit - Save changes to an email
   * @param m   Mailbox
   * @param msg Message to commit
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a m   is not NULL
   * - @a msg is not NULL
   */
  int (*msg_commit)      (struct Mailbox *m, struct Message *msg);

  /**
   * msg_close - Close an email
   * @param m   Mailbox
   * @param msg Message to close
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a m   is not NULL
   * - @a msg is not NULL
   */
  int (*msg_close)       (struct Mailbox *m, struct Message *msg);

  /**
   * msg_padding_size - Bytes of padding between messages
   * @param m Mailbox
   * @retval num Bytes of padding
   *
   * **Contract**
   * - @a m is not NULL
   */
  int (*msg_padding_size)(struct Mailbox *m);

  /**
   * msg_save_hcache - Save message to the header cache
   * @param m Mailbox
   * @param e Email
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a m is not NULL
   * - @a e is not NULL
   */
  int (*msg_save_hcache) (struct Mailbox *m, struct Email *e);

  /**
   * tags_edit - Prompt and validate new messages tags
   * @param m      Mailbox
   * @param tags   Existing tags
   * @param buf    Buffer to store the tags
   * @param buflen Length of buffer
   * @retval -1 Error
   * @retval  0 No valid user input
   * @retval  1 Buf set
   *
   * **Contract**
   * - @a m   is not NULL
   * - @a buf is not NULL
   */
  int (*tags_edit)       (struct Mailbox *m, const char *tags, char *buf, size_t buflen);

  /**
   * tags_commit - Save the tags to a message
   * @param m Mailbox
   * @param e Email
   * @param buf Buffer containing tags
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a m   is not NULL
   * - @a e   is not NULL
   * - @a buf is not NULL
   */
  int (*tags_commit)     (struct Mailbox *m, struct Email *e, char *buf);

  /**
   * path_probe - Does this Mailbox type recognise this path?
   * @param path Path to examine
   * @param st   stat buffer (for local filesystems)
   * @retval num Type, e.g. #MUTT_IMAP
   *
   * **Contract**
   * - @a path is not NULL
   */
  enum MailboxType (*path_probe)(const char *path, const struct stat *st);

  /**
   * path_canon - Canonicalise a Mailbox path
   * @param buf    Path to modify
   * @param buflen Length of buffer
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a buf is not NULL
   */
  int (*path_canon)      (char *buf, size_t buflen);

  /**
   * path_pretty - Abbreviate a Mailbox path
   * @param buf    Path to modify
   * @param buflen Length of buffer
   * @param folder Base path for '=' substitution
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a buf is not NULL
   */
  int (*path_pretty)     (char *buf, size_t buflen, const char *folder);

  /**
   * path_parent - Find the parent of a Mailbox path
   * @param buf    Path to modify
   * @param buflen Length of buffer
   * @retval  0 Success
   * @retval -1 Failure
   *
   * **Contract**
   * - @a buf is not NULL
   */
  int (*path_parent)     (char *buf, size_t buflen);

  /**
   * path_is_empty - Is the Mailbox empty?
   * @param path Mailbox to check
   * @retval 1 Mailbox is empty
   * @retval 0 Mailbox contains mail
   * @retval -1 Error
   *
   * **Contract**
   * - @a path is not NULL and not empty
   */
  int (*path_is_empty)     (const char *path);
};

/* Wrappers for the Mailbox API, see MxOps */
enum MxCheckReturns      mx_mbox_check      (struct Mailbox *m);
enum MxCheckStatsReturns mx_mbox_check_stats(struct Mailbox *m, int flags);
enum MxCheckReturns      mx_mbox_close      (struct Context **ptr);
struct Context *mx_mbox_open       (struct Mailbox *m, OpenMailboxFlags flags);
enum MxCheckReturns mx_mbox_sync       (struct Mailbox *m);
int             mx_msg_close       (struct Mailbox *m, struct Message **msg);
int             mx_msg_commit      (struct Mailbox *m, struct Message *msg);
struct Message *mx_msg_open_new    (struct Mailbox *m, const struct Email *e, MsgOpenFlags flags);
struct Message *mx_msg_open        (struct Mailbox *m, int msgno);
int             mx_msg_padding_size(struct Mailbox *m);
int             mx_save_hcache     (struct Mailbox *m, struct Email *e);
int             mx_path_canon      (char *buf, size_t buflen, const char *folder, enum MailboxType *type);
int             mx_path_canon2     (struct Mailbox *m, const char *folder);
int             mx_path_parent     (char *buf, size_t buflen);
int             mx_path_pretty     (char *buf, size_t buflen, const char *folder);
enum MailboxType mx_path_probe     (const char *path);
struct Mailbox *mx_path_resolve    (const char *path);
struct Mailbox *mx_resolve         (const char *path_or_name);
int             mx_tags_commit     (struct Mailbox *m, struct Email *e, char *tags);
int             mx_tags_edit       (struct Mailbox *m, const char *tags, char *buf, size_t buflen);

struct Account *mx_ac_find     (struct Mailbox *m);
struct Mailbox *mx_mbox_find   (struct Account *a, const char *path);
struct Mailbox *mx_mbox_find2  (const char *path);
bool            mx_mbox_ac_link(struct Mailbox *m);
bool            mx_ac_add      (struct Account *a, struct Mailbox *m);
int             mx_ac_remove   (struct Mailbox *m);

int                 mx_access           (const char *path, int flags);
void                mx_alloc_memory     (struct Mailbox *m);
int                 mx_path_is_empty    (const char *path);
void                mx_fastclose_mailbox(struct Mailbox *m);
const struct MxOps *mx_get_ops          (enum MailboxType type);
bool                mx_tags_is_supported(struct Mailbox *m);

#endif /* MUTT_MX_H */
