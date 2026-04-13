#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <cmath>
#include <unordered_map>

#define LOG_INFO(x)     std::cout << x << "\n"
#define LOG_PROCESS(x)  std::cout << "🔄 " << x << "\n"
#define LOG_ERROR(x)    std::cerr << "❌ " << x << "\n"
#define LOG_WARN(x)     std::cout << "⚠️  " << x << "\n"
#define LOG_SUCCESS(x)  std::cout << "✅ " << x << "\n"

/**
 * Написати консольну C++ програму, яка розраховує точку скиду боєприпасу з дрона на задану наземну ціль. 
 * Програма враховує: 
 *      - тип боєприпасу, 
 *      - його фізичні параметри (масу, drag-коефіцієнт, підйомну силу планерування), 
 *      - швидкість дрона
 *      - розгінну дистанцію.
 * 
 * Програма виводить у output.txt:
 *      - Проміжну точку маневру, якщо h + accelerationPath > D
 *      - Координати точки скиду (fireX, fireY)
 */

/**
 *      | Параметр          | Тип    | Опис                                   |
 *      -----------------------------------------------------------------------
 *      | xd, yd, zd        | float  | Координати дрона (zd — висота, м)      |
 *      | targetX, targetY  | float  | Координати цілі на землі (z = 0)       |
 *      | attackSpeed       | float  | Швидкість атаки дрона (м/с)            |
 *      | accelerationPath  | float  | Довжина розгону дрона перед скидом (м) |
 *      | ammo_name         | char[] | Назва боєприпасу                       |
 * 
 *      Приклад вхідного файлу: 
 *          100 100 100 200 200 10 10 VOG-17 
 *      Приклад вихідного файлу: 
 *          173.759 173.759
 */

struct InputData {
    float xd; 
    float yd; 
    float zd;
    float targetX; 
    float targetY;
    float attackSpeed;
    float accelerationPath;
    std::string ammo_name;
};

 /**
  *     | Назва       | m (кг) | d (drag) | l (lift) | Тип               |
  *     ------------------------------------------------------------------
  *     | VOG-17      | 0.35   | 0.07     | 0.0      | Вільне падіння    |
  *     | M67         | 0.6    | 0.10     | 0.0      | Вільне падіння    |
  *     | RKG-3       | 1.2    | 0.10     | 0.0      | Вільне падіння    |
  *     | GLIDING-VOG | 0.45   | 0.10     | 1.0      | Планеруючий       |
  *     | GLIDING-RKG | 1.4    | 0.10     | 1.0      | Планеруючий       |
  * 
  *     m — маса боєприпасу (кг)
  *     d — коефіцієнт аеродинамічного опору
  *     l — коефіцієнт підйомної сили (0 = вільне падіння, 1 = планерування).
  */

struct AmmoInfo {
    float m;
    float d;
    float l;
    bool isFreeFall;
};

std::unordered_map<std::string, AmmoInfo> ammo_types_info = {
    {"VOG-17",      {0.35f, 0.07f, 0.0f, true}},
    {"M67",         {0.6f,  0.1f,  0.0f, true}},
    {"RKG-3",       {1.2f,  0.1f,  0.0f, true}},
    {"GLIDING-VOG", {0.45f, 0.1f,  1.0f, false}},
    {"GLIDING-RKG", {1.4f,  0.1f,  1.0f, false}},
};

bool getAmmoInfoByType(const std::string ammo_name, AmmoInfo& outAmmo)
{
    auto it = ammo_types_info.find(ammo_name);
    
    if (it == ammo_types_info.end()) {
        LOG_WARN("Unknown ammo type: " << ammo_name);
        return false;
    }

    LOG_SUCCESS("Successfully found ammo type.");
    outAmmo = it->second;

    std::cout << "📄 Result:\n";
    std::cout << "  - m: " << outAmmo.m << "\n";
    std::cout << "  - d: " << outAmmo.d << "\n";
    std::cout << "  - l: " << outAmmo.l << "\n";
    std::cout << "  - isFreeFall: " << (outAmmo.isFreeFall ? "true" : "false") << "\n";

    return true;
}

bool getInputData(const std::string& file_name, InputData& inputData)
{
    LOG_PROCESS("Reading " + file_name + "...");

    std::ifstream file(file_name);
    if (!file) {
        LOG_ERROR("Error opening file");
        return false;
    }
    
    // Example: "100 100 100 200 200 10 10 VOG-17"
    file >> inputData.xd >> inputData.yd >> inputData.zd 
        >> inputData.targetX >> inputData.targetY 
        >> inputData.attackSpeed 
        >> inputData.accelerationPath 
        >> inputData.ammo_name;

    if (file.fail()) {
        LOG_ERROR("Invalid format: wrong data types");
        return false;
    }
    else
    {
        LOG_SUCCESS("Successfully found all params.");
    }

    std::cout << "📄 Result:\n";
    std::cout << "  - xd: " << inputData.xd << "\n";
    std::cout << "  - yd: " << inputData.yd << "\n";
    std::cout << "  - zd: " << inputData.zd << "\n";
    std::cout << "  - targetX: " << inputData.targetX << "\n";
    std::cout << "  - targetY: " << inputData.targetY << "\n";
    std::cout << "  - attackSpeed: " << inputData.attackSpeed << "\n";
    std::cout << "  - accelerationPath: " << inputData.accelerationPath << "\n";
    std::cout << "  - ammo_name: " << inputData.ammo_name << "\n";

    std::string extra;
    if (file >> extra) {
        LOG_WARN("Found extra data: " + extra + " (ignored)");
    }

    return true;
}

int main()
{
    std::string file_name = "input.txt";

    InputData inputData{};
    if (!getInputData(file_name, inputData))
    {
        return 1;
    }

    AmmoInfo ammoInfo;
    if (!getAmmoInfoByType(inputData.ammo_name, ammoInfo)) 
    {
        return 1;
    }

    return 0;
}


 /**
 * Step 1: Обчислити час польоту снаряда t
 * Ми маємо координати дрона коли він не рухається, ми знаємо відстань протягом якої він набере швидкість від 0 для атаки.
 * Ми також маємо тип снаряду і його параметри: масу (m), опір повітря (d) та коефіцієнт підйомної сили (l)
 * Тому на цьому кроці ми шукаємо скільки часу снаряд буде летіти, з певної висоти (zd - висота на якій знаходиться дрон в момент скиду снаряду)
 * маючи його початкову вертикальну швидкість (0 m/s в момент скиду) та горизонтальну швидкість (attackSpeed m/s - швидкість дрона в момент скиду)
 * Важливо: цей час t - це час від моменту відділення снаряда від дрона до моменту його падіння на землю.
 * 
 * Step 2: Обчислити горизонтальну дальність польоту h
 * Наступне що ми можемо зробити - знайти яку горизонтальну відстань снаряд пролетить протягом часу t.
 * Вертикально він пролетить відстань від висоти дрона (zd) до землі (0), але це вже враховано в t.
 * Тут нас цікавить тільки горизонтальний рух.
 * 
 * Step 3: Обчислити відстань від дрона до цілі D
 * На цьому кроці ми знаходимо яка відстань від дрона до цілі. Це горизонтальна пряма між 2 точками, не по діагоналі.
 * Тобто в декартовій системі координат (x та y). Тобто відстань між координатами x та y дрона (xd, yd) та цілі (targetX, targetY).
 * 
 * Step 4: Перевірити необхідність маневру 
 * Тепер важливо перевірити чи на достатній відстані дрон зараз знаходиться від цілі, ще до скиду.
 * Тобто якщо ми будемо занадто близько, тоді снаряд може перелетіти або не встигнути правильно лягти в траєкторію.
 * Для цього ми перевіряємо умову:
 *      (горизонтальна дальність польоту снаряда + відстань для розгону дрона) > відстані від дрона до цілі
 *              або
 *      (h + accelerationPath) > D
 * Якщо дрон заблизько - значить йому потрібно спочатку відійти/перелетіти на правильну дистанцію перед виконанням скиду.
 * 
 * Step 5: Знайти точку скиду
 * Це останній крок. Ми знаходимо ті координати для дрона, в яких він має скинути снаряд.
 * Отже, снаряд після скиду пролетить горизонтальну відстань h, тому ми повинні знайти точку,
 * яка знаходиться на відстані h перед ціллю вздовж напрямку "дрон → ціль".
 * Тобто ми рухаємось назад по прямій лінії між дроном і ціллю.
 */

 