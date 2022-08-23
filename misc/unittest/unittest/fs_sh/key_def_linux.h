#ifndef _KEY_DEF_LINUX_H_
#define _KEY_DEF_LINUX_H_

#define CTRL_KEY (1<<31)

#define VK_ESC 27
#define VK_RETURN ((uint)'\n')
#define VK_CTRL_W 23
#define VK_CTRL_E 5
#define VK_CTRL_R 18
#define VK_CTRL_T 20
#define VK_CTRL_Y 25
#define VK_CTRL_U 21
#define VK_CTRL_I ((uint)'\t')
#define VK_CTRL_O 15
#define VK_CTRL_P 16
#define VK_CTRL_A 1
#define VK_CTRL_D 4
#define VK_CTRL_F 6
#define VK_CTRL_G 7 //'\a'
#define VK_CTRL_H 8
#define VK_CTRL_J ((uint)'\n')
#define VK_CTRL_K 11 //'\v'
#define VK_CTRL_L 12 //'\f'
#define VK_CTRL_X 24
#define VK_CTRL_V 22
#define VK_CTRL_B 2
#define VK_CTRL_N 14
#define VK_CTRL_M '\n'

#define VK_BACKSPACE ((uint)'\b')
#define VK_BACKSPACE_2 ((uint)127)
#define VK_DELETE (CTRL_KEY|(uint)'3')
#define VK_UP (CTRL_KEY|(uint)'A')
#define VK_DOWN (CTRL_KEY|(uint)'B')
#define VK_LEFT (CTRL_KEY|(uint)'D')
#define VK_RIGHT (CTRL_KEY|(uint)'C')
#define VK_PGUP (CTRL_KEY|(uint)'5')
#define VK_PGDN (CTRL_KEY|(uint)'6')
#define VK_HOME (CTRL_KEY|(uint)'1')
#define VK_END (CTRL_KEY|(uint)'4')
#define VK_HOME_2 (CTRL_KEY|(uint)'H')
#define VK_END_2 (CTRL_KEY|(uint)'F')

#endif
