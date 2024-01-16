__asm("jmp kmain");
#define VIDEO_BUF_PTR (0xb8000)
#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)
// Селектор секции кода, установленный загрузчиком ОС
#define GDT_CS (0x8)
#define PIC1_PORT (0x20)
// Базовый порт управления курсором текстового экрана. Подходит для большинства, но может отличаться в других BIOS и в общем случае адрес должен быть прочитан из BIOS data area. 
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) // Ширина текстового экрана
#define ALPHABET_SIZE 256
#define max(a, b) ((a > b) ? a : b)
unsigned int strnum_ = 2;			// Текущая строка курсора
unsigned int pos_ = 0;				// Текущая позиция курсора в строке strnum_
unsigned char tmpl[40] = {'\0'};	// Текущий template
int *param = (int *) 0x9004; 		// Переменная, которая хранит в себе параметр, введённый на этапе загрузчика
// Массив для перевода скан-кодов в ASCII таблицу
char kbd_US [128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     
    0, /* <-- control key */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  
	15,  /* Shift */
  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};
unsigned char all_comm[8][40]	// Таблица комманд
{
	"help",
	"info",
	"upcase",
	"downcase",
	"titlize",
	"template",
	"search",
	"shutdown"
};
enum comm_numb					// Перечисление для команд в switch-case
{
	HELP,
	INFO,
	UPCASE,
	DOWNCASE,
	TITLIZE,
	TEMPLATE,
	SEARCH,
	SHUTDOWN
};
// Функция вывода строки на экран
void out_str(int color, const char* ptr, unsigned int strnum)
{
	unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
	video_buf += strnum * VIDEO_WIDTH * 2;
	while (*ptr)
	{
		if (*ptr == '\n')
		{
			strnum++;
			strnum_++;
			video_buf = (unsigned char*) VIDEO_BUF_PTR + strnum * VIDEO_WIDTH * 2;
		}
		else 
		{
			video_buf[0] = (unsigned char) *ptr; 
			video_buf[1] = color;
			video_buf += 2;
		}
		ptr++;
	}
}
// Функция вывода символа на экран
void out_symb(int color, unsigned char symb, unsigned int strnum, unsigned int pos)
{
	unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
	video_buf += (strnum * VIDEO_WIDTH*2) + pos * 2;
	video_buf[0] = symb; 
	video_buf[1] = color;
}
// Структура описывает данные об обработчике прерывания
struct idt_entry
{
	unsigned short base_lo; // Младшие биты адреса обработчика
	unsigned short segm_sel; // Селектор сегмента кода
	unsigned char always0; // Этот байт всегда 0
	unsigned char flags; // Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
	unsigned short base_hi; // Старшие биты адреса обработчика
} __attribute__((packed)); // Выравнивание запрещено

// Структура, адрес которой передается как аргумент команды lidt
struct idt_ptr
{
	unsigned short limit;
	unsigned int base;
} __attribute__((packed)); // Выравнивание запрещено

struct idt_entry g_idt[256]; // Реальная таблица IDT

struct idt_ptr g_idtp; // Описатель таблицы для команды lidt

// Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону
void default_intr_handler()
{
	asm("pusha");
	// ... (реализация обработки)
	asm("popa; leave; iret");
}
// Регистрация определённого прерывания, задаваемого программистом
typedef void (*intr_handler)();
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
	unsigned int hndlr_addr = (unsigned int) hndlr;
	g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
	g_idt[num].segm_sel = segm_sel;
	g_idt[num].always0 = 0;
	g_idt[num].flags = flags;
	g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}

// Функция инициализации системы прерываний: заполнение массива с адресами обработчиков
void intr_init()
{
	int i;
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
	for(i = 0; i < idt_count; i++)
		intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start()
{
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
	g_idtp.base = (unsigned int) (&g_idt[0]);
	g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
	asm("lidt %0" : : "m" (g_idtp) );
}
// Включить прерывания
void intr_enable()
{
	asm("sti");
}
// Выключить прерывания
void intr_disable()
{
	asm("cli");
}
static inline unsigned char inb (unsigned short port) // Чтение из порта
{
	unsigned char data;
	asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
	return data;
}
static inline void outb (unsigned short port, unsigned char data) // Запись в порт байта
{
	asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}
static inline void outw (unsigned short port, unsigned short int data) // Запись в порт двух байтов
{
	asm volatile ("outw %0, %1" : : "a" (data), "d" (port));
}
// Функция переводит курсор на строку strnum (0 – самая верхняя) в позицию pos на этой строке (0 – самое левое положение).
void cursor_moveto(unsigned int strnum, unsigned int pos)
{
	unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
	outb(CURSOR_PORT, 0x0F);
	outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
	outb(CURSOR_PORT, 0x0E);
	outb(CURSOR_PORT + 1, (unsigned char)( (new_pos >> 8) & 0xFF));
}


unsigned char command[40];		// Хранение текущей команды
int ind = 0;					
int shift = 0;					// Флаг нажатия кнопки SHIFT
// Очистка текстового экрана
void clear()
{
	unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
	for (int i = 0; i < 2000; i++)
	{
		video_buf[0] = ' '; 
		video_buf[1] = 0x07;
		video_buf += 2;
	}
}
// Сравнение строк
int strcmp_(unsigned char *s1, unsigned char *s2)
{
	int i = 0;
	while (s1[i] != '\0' && s2[i] != '\0')
	{
		if (s1[i] != s2[i])
			break;
		i++;
	}
	return !(s1[i] == '\0' && s2[i] == '\0');
}
// Копирование строки из src в dst
void strcpy_(unsigned char *dst, const char *src) {
	int i = 0;
    while (src[i] != '\0')
	{
		dst[i] = src[i];
		i++;
	}
    dst[i] = '\0';  // добавляем нулевой символ
}
// Добавление второй строки к концу первой
void strcat_(unsigned char *str1, const char *str2)
{
    unsigned char* begin = str1;
    while (*str1)
        str1++;
 
    while(*str1++ = *str2++);
 
    *str1 = '\0';
}
// Вычисление длины строки
int strlen_(unsigned char *str1)
{
	int len = 0;
	while (*str1++) len++;
	return len - 1;
}
// Функция для замены двух чисел
void swap(char *x, char *y) {
    char t = *x; *x = *y; *y = t;
}
 
// Функция для обращения `buffer[i…j]`
char* reverse(char *buffer, int i, int j)
{
    while (i < j) {
        swap(&buffer[i++], &buffer[j--]);
    }
 
    return buffer;
}
 
// Итеративная функция для реализации функции `int_to_char()` в C
char* int_to_char(int value, char* buffer, int base)
{
    // неправильный ввод
    if (base < 2 || base > 32) {
        return buffer;
    }
 
    // считаем абсолютное значение числа
    int n = value;
 
    int i = 0;
    while (n)
    {
        int r = n % base;
 
        if (r >= 10) {
            buffer[i++] = 65 + (r - 10);
        }
        else {
            buffer[i++] = 48 + r;
        }
 
        n = n / base;
    }
 
    // если число равно 0
    if (i == 0) {
        buffer[i++] = '0';
    }
 
    // Если основание равно 10 и значение отрицательное, результирующая строка
    // предшествует знак минус (-)
    // При любой другой базе значение всегда считается беззнаковым
    if (value < 0 && base == 10) {
        buffer[i++] = '-';
    }
 
    buffer[i] = '\0'; // нулевая завершающая строка
 
    // переворачиваем строку и возвращаем ее
    return reverse(buffer, 0, i - 1);
}
// Функция заполнения таблицы смещений для Б-М
void precompute_bad_chars(char *str, int size, int bad_char_table[]) {
    // Заполняем таблицу смещений для всех символов алфавита
    for (int i = 0; i < ALPHABET_SIZE; i++)
        bad_char_table[i] = -1;
  
    // Заполняем таблицу смещений только для символов из подстроки
    for (int i = 0; i <= size; i++)
        bad_char_table[(int) str[i]] = i;
}
int bad_char_table[ALPHABET_SIZE];
// Исполнение команды
void action()
{
	unsigned char temp_comm[40];
	int i = 0;
	while (command[i] != ' ' && i < 39)
	{
		temp_comm[i] = command[i];
		i++;
	}
	temp_comm[i++] = '\0';
	
	int ans;
	for (ans = 0; ans < 8; ans++)
	{
		if(strcmp_(temp_comm, all_comm[ans]) == 0)
			break;
	}
	switch (ans)
	{
		case HELP:
		{
			const char *text = "Command List:\n- help: displays a list of commands.\n- info: Displays information about the author and OS development tools.\n- upcase [string]: Prints the specified string in upper case.\n- donwcase [string]: Prints the specified string in lower case.\n- titlize [string]: Prints the string, the first letter in each word is big.\n- template [template]: Load a template for searching substrings into memory.\n- search [string]: First occurrence of the temp in the specified string.\n- shutdown: Shut down the computer";
			out_str(0x07, text, ++strnum_);
			break;
		}
		case INFO:
		{
			unsigned char text[256] = "String OS: v.01. Developer: Vyacheslav Totski, 4851003/10002, SpbPU, 2023\nCompilers: bootloader: GNU (Intel), kernel: gcc\nBootloader parameters: ";
			if (*param==0)
				strcat_(text, "std");
			else
				strcat_(text, "bm");
			out_str(0x07, (const char*)text, ++strnum_);
			break;
		}
		case UPCASE:
		{
			unsigned char symb;
			strcpy_(temp_comm, (const char*)(&command[i]));
			i = 0;
			while (temp_comm[i] != '\0')
			{
				if (temp_comm[i] >= 'a' && temp_comm[i] <= 'z')
					temp_comm[i]-= 32;
				i++;
			}
			out_str(0x07, (const char*)temp_comm, ++strnum_);
			break;
		}
		case DOWNCASE:
		{
			unsigned char symb;
			strcpy_(temp_comm, (const char*)(&command[i]));
			i = 0;
			while (temp_comm[i] != '\0')
			{
				if (temp_comm[i] >= 'A' && temp_comm[i] <= 'Z')
					temp_comm[i]+= 32;
				i++;
			}
			out_str(0x07, (const char*)temp_comm, ++strnum_);
			break;
		}
		case TITLIZE:
		{
			unsigned char symb;
			strcpy_(temp_comm, (const char*)(&command[i]));
			i = 0;
			if (temp_comm[i] >= 'a' && temp_comm[i] <= 'z' )
				temp_comm[i++] -= 32;
				
			while (temp_comm[i] != '\0')
			{
				if (temp_comm[i] >= 'a' && temp_comm[i] <= 'z' && temp_comm[i-1] == ' ')
					temp_comm[i] -= 32;
				i++;
			}
			
			out_str(0x07, (const char*)temp_comm, ++strnum_);
			break;
		}
		case TEMPLATE:
		{
			strcpy_(tmpl, (const char*)(&command[i]));
			i = 0;
			unsigned char text[40] = "Template '\0";
			strcat_(text, (const char*)tmpl);
			strcat_(text, "' loaded.");
			if (*param == 0)
			{
				out_str(0x07, (const char*)text, ++strnum_);
				break;
			}
			int len1 = strlen_(tmpl);
			char c[2];
			precompute_bad_chars((char*)tmpl, len1, bad_char_table);
			strcat_(text, " BM info:");
			out_str(0x07, (const char*)text, ++strnum_);
			strnum_++;
			for (int index = 0; index <= len1; index++)
			{
				out_symb(0x07, tmpl[index], strnum_, pos_++);
				out_symb(0x07, ':', strnum_, pos_++);
				char b[2];
				out_symb(0x07, int_to_char(len1 + 1 - bad_char_table[(int) tmpl[index]], b, 10)[0], strnum_, pos_++);
				out_symb(0x07, ' ', strnum_, pos_++);
			}
			pos_ = 0;
			break;
		}
		case SEARCH:
		{
			int index = 0;
			int answer = 0;
			unsigned char text[40];
			strcpy_(text, (const char*)(&command[i]));
			int len1 = strlen_(tmpl);
			int len2 = strlen_(text);
			if (*param == 0)
			{
				int j;
				for (index = 0; index <= len2 - len1; index++)
				{
					
					for (j = 0; j < len1 + 1 && text[index + j] == tmpl[j]; j++);
					if (j == len1 + 1)
					{
						answer = 1;
						break;
					}
				}
			}
			else
			{
				while (index <= len2 - len1)
				{
					int j = len1;
					while (j >= 0 && tmpl[j] == text[index + j])
						j--;
					if (j < 0)
					{
						answer = 1;
						break;
					}
					index += max(1, j - bad_char_table[text[index + j]]);
				}
			}
			if (answer)
			{
				strcpy_(text, "Found '");
				strcat_(text, (const char*)tmpl);
				strcat_(text, "' at pos: ");
				char a[2];
				int_to_char(index, a, 10);
				strcat_(text, (const char*)a);
				out_str(0x07, (const char*)text, ++strnum_);
			}
			else
			{
				strcpy_(text, "Not found '");
				strcat_(text, (const char*)tmpl);
				strcat_(text, "'");
				out_str(0x07, (const char*)text, ++strnum_);
			}
			
			break;
		}
		case SHUTDOWN:
		{
			const char *text = "Powering off...";
			out_str(0x07, text, ++strnum_);
			outw(0x604, 0x2000);
			break;
		}
		default:
			const char *text = "Error: command not recognized";
			out_str(0x07, text, ++strnum_);
	}
}
// Обработка нажатой клавиши
void on_key(unsigned char scan_code) {

	unsigned char symb = kbd_US[scan_code];
	if (strnum_>=24)
	{
		strnum_ = 0;
		clear();
	}
	// BackSpace
	if (symb == '\b')
	{	
		if (pos_ > 0)
		{
			pos_--;
			command[ind--] = '\0';
		}
		out_symb(0x07, ' ', strnum_, pos_);
		cursor_moveto(strnum_, pos_);
	}
	// Enter
	if (symb == '\n')
	{
		pos_ = 0;
		
		if (command[0] != '\0')
		{	
			action();
			command[0] = '\0';
			ind = 0;
			strnum_++;
		}
		strnum_++;
		cursor_moveto(strnum_, pos_);
	}
	// Ограничение ввода больше 40-ка символов
	if (pos_ >= 40)
	{
		return;
	}
		if (symb == 15)
	{
		shift = 1;
	}
	// a-z, 1-0
	if (symb >= '!' && symb <= '~' || symb == ' ')
	{
		if (shift)
		{
			if (symb >= 'a' && symb <= 'z')
				symb-=32;
		}
		command[ind++] = symb;
		command[ind] = '\0';
		out_symb(0x07, symb, strnum_, pos_);
		cursor_moveto(strnum_, ++pos_);
	}

}
// Обработка отпущенных клавиш
void un_key(unsigned char scan_code)
{
	if (scan_code == 170)
		shift = 0;
}
// Считывание скан-кода нажатой или отпущенной клавиши
void keyb_process_keys()
{
	// Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
	if (inb(0x64) & 0x01) 
	{
		unsigned char scan_code;
		unsigned char state;
		scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
		if (scan_code < 128) // Скан-коды выше 128 - это отпускание клавиши
			on_key(scan_code);
		else 
			un_key(scan_code);
	}
}

void keyb_handler()
{
	asm("pusha");
	// Обработка поступивших данных
	keyb_process_keys();
	// Отправка контроллеру 8259 нотификации о том, что прерывание обработано 
	outb(PIC1_PORT, 0x20); 
	asm("popa; leave; iret");
}
void keyb_init()
{
	// Регистрация обработчика прерывания
	intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler); 
	// segm_sel=0x8, P=1, DPL=0, Type=Intr
	// Разрешение только прерываний клавиатуры от контроллера 8259
	outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - все прерывания, 0x02 - бит IRQ1 (клавиатура). 
	// Разрешены будут только прерывания, чьи биты установлены в 0
}
extern "C" int kmain()
{
	unsigned char hello[40] = "Welcome to StringOS!";
	out_str(0x07, (const char*)hello, 0);
	strcpy_(hello, "To list commands, write help\0");
	out_str(0x07, (const char*)hello, 1);
	cursor_moveto(strnum_, pos_);
	intr_enable();
	keyb_init();
	intr_start();
	asm("hlt");

	return 0;
}
