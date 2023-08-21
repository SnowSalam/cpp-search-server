// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь: 271
#include <iostream>
using std::cout;

int main() {
	int number = 1, count = 0;
	while (number != 1000) {
		if (number % 10 == 3 or (number / 10 % 10) == 3 || (number / 100) == 3) count++;
		number++;
	}
	cout << "Answer is: " << count;
	return 0;
}
// Закомитьте изменения и отправьте их в свой репозиторий.
