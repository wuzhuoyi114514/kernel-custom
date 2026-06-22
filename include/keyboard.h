#ifndef KEYBOARD_H
#define KEYBOARD_H

// 声明函数，让其他文件（如内核入口文件）能看见这些函数
char keyboard_buffer_pop();
void keyboard_init(); // 如果你有初始化函数的话

#endif
