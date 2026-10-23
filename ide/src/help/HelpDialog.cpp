#include "help/HelpDialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

namespace {

struct HelpSection {
  const char *title;
  const char *html;
};

constexpr HelpSection kSections[] = {
    {"Огляд", R"HTML(
<h1>Що таке calx IDE</h1>
<p><b>calx-ide</b> — графічне середовище для мови <b>calx</b>: редактор файлів
<code>.calc</code>, інтерактивний REPL і покроковий дебаг. Ядро те саме, що в
консольному <code>calx</code> (бібліотека <code>calc</code>).</p>
<p>Два режими:</p>
<ol>
<li><b>Редактор</b> — файли, Запуск, Дебаг, підсвітка, доповнення, панелі.</li>
<li><b>Інтерактив</b> — сесійний REPL з історією <code>~/.calx_history</code>.</li>
</ol>
<div class="ok">Консольний <code>calx</code> лишається окремим бінарником;
IDE збирається опцією <code>CALX_BUILD_IDE=ON</code>.</div>
<p>Меню: <b>Файл</b>, <b>Редагування</b>, <b>Виконання</b>, <b>Вигляд</b>,
<b>Довідка</b>.</p>
)HTML"},
    {"З чого почати", R"HTML(
<h1>З чого почати</h1>
<ol>
<li>Запустіть <code>calx-ide</code> або <code>calx-ide program.calc</code>.</li>
<li>У <b>Редакторі</b> введіть:
<pre>let x = 2 + 2;
print("x = {}", x);</pre></li>
<li><b>Ctrl+R</b> (Запуск) — вивід у <b>Консолі</b>, змінні в панелі <b>Змінні</b>.</li>
<li>Режим <b>Інтерактив</b> — ті самі рядки з підказкою <code>calx&gt;</code>.</li>
<li><b>Ctrl+I</b> — повне форматування коду (див. розділ «Форматування»).</li>
</ol>
<div class="tip"><b>F1</b> — ця довідка. <b>Вигляд → Світла / темна тема</b> —
палітра вікна.</div>
)HTML"},
    {"Редактор", R"HTML(
<h1>Режим редактора</h1>
<ul>
<li><b>Файл</b> — Новий / Відкрити / Зберегти / Зберегти як (<code>.calc</code>).</li>
<li><b>Підсвітка</b> — ключові слова, літерали, рядки, builtins, константи, <code>//</code>.</li>
<li><b>Доповнення</b> — під час набору або <b>Ctrl+Space</b>.</li>
<li><b>Hover</b> — підказка над builtin / константою / ключовим словом.</li>
<li><b>Tab</b> — два пробіли; <b>Enter</b> — автовідступ за <code>{ ( [</code>.</li>
<li><b>Форматувати</b> (<b>Ctrl+I</b>) — parse → pretty-print (пробіли, відступи,
<code>else if</code>, коментарі). Некоректний код не змінюється — помилка в Консолі.</li>
<li><b>Форматувати при збереженні</b> — опція в меню Редагування; запамʼятовується
між запусками. Якщо розбір не вдався — запит, чи зберігати як є.</li>
<li>Помилка компіляції — хвилясте підкреслення + текст у Консолі.</li>
</ul>
<div class="ok">Клік у gutter або <b>F9</b> — breakpoint.</div>
)HTML"},
    {"Інтерактив", R"HTML(
<h1>Інтерактив (REPL)</h1>
<ul>
<li>Успішні рядки <b>накопичуються в сесію</b> (<code>let</code> / <code>fun</code> лишаються).</li>
<li>Незакриті <code>{ ( [</code> — підказка <code>...&gt;</code>.</li>
<li><b>Enter</b> — виконати; <b>Shift+Enter</b> — новий рядок у полі вводу.</li>
<li><b>↑ / ↓</b> — історія (<code>~/.calx_history</code>).</li>
<li>Команди: <code>:help</code>, <code>:clear</code>.</li>
</ul>
<div class="tip">У REPL крапку з комою в кінці виразу можна опустити.
Після блоків <code>fun</code>/<code>if</code>/<code>while</code>/<code>for</code>
вона не додається. У <b>файлах</b> <code>;</code> обовʼязкова.</div>
<pre>calx&gt; let a = 12 * (3 + 2)
calx&gt; print("sum = {}", a + -1.23)
sum = 58.77
calx&gt; fun add(x, y) {
...&gt;   return x + y;
...&gt; }
calx&gt; print(add(2, 3))
5</pre>
)HTML"},
    {"Дебаг", R"HTML(
<h1>Дебаг</h1>
<ol>
<li>Breakpoints: gutter або <b>F9</b>.</li>
<li><b>Shift+F5</b> — старт дебагу (пауза на початку).</li>
<li><b>F5</b> — Continue; <b>F11</b> Step Into; <b>F10</b> Step Over;
<b>Shift+F11</b> Step Out.</li>
<li><b>Пауза</b> / <b>Стоп</b> — перервати або завершити сесію.</li>
</ol>
<p>На паузі: поточний рядок, локалі кадру, дізасемблер з <code>ip</code>.
ВМ у фоновому потоці.</p>
<div class="warn">Breakpoints — за номером рядка (source map). Точність Step Over
залежить від карт рядків у складних виразах.</div>
)HTML"},
    {"Панелі", R"HTML(
<h1>Панелі</h1>
<ul>
<li><b>Консоль</b> — stdout і помилки.</li>
<li><b>Змінні</b> — імʼя, тип, значення після Run / на паузі.</li>
<li><b>Функції</b> — користувацькі <code>fun</code> (арність, entry).</li>
<li><b>Константи</b> — вбудовані (<code>pi</code>, <code>c</code>, <code>NA</code>…).</li>
<li><b>Дізасемблер</b> — дамп байткоду.</li>
</ul>
<p>Показати/сховати — <b>Вигляд</b>.</p>
)HTML"},
    {"Гарячі клавіші", R"HTML(
<h1>Гарячі клавіші</h1>
<table>
<tr><td><b>F1</b></td><td>Довідка</td></tr>
<tr><td><b>Ctrl+R</b></td><td>Запуск</td></tr>
<tr><td><b>F5</b></td><td>Continue / запуск без дебагу</td></tr>
<tr><td><b>Shift+F5</b></td><td>Почати дебаг</td></tr>
<tr><td><b>F9</b></td><td>Breakpoint</td></tr>
<tr><td><b>F10</b> / <b>F11</b></td><td>Step Over / Into</td></tr>
<tr><td><b>Shift+F11</b></td><td>Step Out</td></tr>
<tr><td><b>Ctrl+I</b></td><td>Форматувати</td></tr>
<tr><td><b>Ctrl+Space</b></td><td>Доповнення</td></tr>
<tr><td><b>Ctrl+N/O/S</b></td><td>Новий / Відкрити / Зберегти</td></tr>
</table>
)HTML"},
    {"Форматування", R"HTML(
<h1>Форматування коду</h1>
<p><b>Редагування → Форматувати</b> (<b>Ctrl+I</b>) парсить програму в AST і
друкує її канонічно:</p>
<ul>
<li>відступи 2 пробіли; блоки <code>{ ... }</code> на окремих рядках;</li>
<li>пробіли навколо бінарних операторів (<code>a + b</code>, <code>x == 1</code>);</li>
<li><code>and</code> / <code>or</code> / <code>not</code> з пробілами;</li>
<li>коми в списках аргументів, масивах, записах: <code>f(a, b)</code>,
<code>[1, 2]</code>, <code>{x: 1, y: 2}</code>;</li>
<li>ланцюжки <code>else if</code>;</li>
<li>дужки за пріоритетом (щоб зберегти сенс виразу);</li>
<li>коментарі <code>//</code> зберігаються (окремі рядки й хвостові).</li>
</ul>
<div class="warn">Якщо код не розбирається — форматування <b>не змінює</b> буфер,
помилка зʼявляється в Консолі. При автоформаті на збереженні можна все одно
зберегти незмінений текст.</div>
<p>Увімкнути автоформат: <b>Редагування → Форматувати при збереженні</b>.</p>
<pre>// було
let x=1+2*3;if(x&gt;0){print(x);}

// стало
let x = 1 + 2 * 3;
if (x &gt; 0) {
  print(x);
}</pre>
)HTML"},
    {"Типи і літерали", R"HTML(
<h1>Типи і літерали</h1>
<p>Тип виводиться з виразу. Підтримуються:</p>
<table>
<tr><td><b>Boolean</b></td><td><code>true</code>, <code>false</code></td></tr>
<tr><td><b>Integer</b></td><td><code>12</code>, <code>-3</code> (<code>int64</code>)</td></tr>
<tr><td><b>Rational</b></td><td><code>1/2</code>, <code>3/4</code> — <b>без</b> пробілів навколо <code>/</code></td></tr>
<tr><td><b>Real</b></td><td><code>2.0</code>, <code>.5</code>, <code>2e1</code> (<code>long double</code>)</td></tr>
<tr><td><b>Complex</b></td><td><code>2i</code>, <code>2.5i</code>, <code>2 + 2i</code></td></tr>
<tr><td><b>Array</b></td><td><code>[1, 2]</code>, <code>[]</code></td></tr>
<tr><td><b>Record</b></td><td><code>{x: 1}</code>, <code>{}</code></td></tr>
</table>
<div class="tip"><code>1/2</code> — літерал-дріб; <code>1 / 2</code> — ділення → Real
<code>0.5</code>.</div>
<pre>let q = 3/4;
let z = 1 + 2i;
print(q);
print(z);</pre>
<p>Коментар: <code>//</code> до кінця рядка. Рядкові значення змінних
<strong>немає</strong> — рядки лише як формат у <code>print</code>.</p>
)HTML"},
    {"Змінні", R"HTML(
<h1>Змінні і присвоєння</h1>
<pre>let name = вираз;
let a = 1, b = a * 2, c = -3.5;
name = новий_вираз;</pre>
<ul>
<li>Ініціалізатор обовʼязковий (<code>let a;</code> — помилка).</li>
<li>Повторне імʼя в тій самій області — помилка.</li>
<li>Імʼя видиме лише <b>після</b> свого ініціалізатора:
<code>let a = 2, b = a * 3</code> — ок; <code>let a = a + 1</code> — ні.</li>
<li>Ідентифікатори: <code>[A-Za-z_][A-Za-z0-9_]*</code>.</li>
<li>Присвоєння — лише вже оголошеній змінній (або <code>a[i]</code>, <code>a.field</code>).</li>
</ul>
<pre>let i = 0;
i = i + 1;
print(i);</pre>
)HTML"},
    {"Вивід print", R"HTML(
<h1>Вивід: print</h1>
<pre>print(вираз);
print("формат", аргументи...);</pre>
<ul>
<li>Без формату — значення і новий рядок.</li>
<li>З форматом — як <code>std::print</code> / <code>std::format</code>:
<code>{}</code>, <code>{0}</code>, <code>{:.2f}</code>, <code>{:d}</code>,
<code>{:x}</code>, <code>{{</code>/<code>}}</code>.</li>
<li>Автоматичну й ручну індексацію змішувати не можна.</li>
<li>Кількість аргументів перевіряється на компіляції.</li>
<li>Escape у рядку: <code>\n</code> <code>\t</code> <code>\r</code> <code>\\</code> <code>\"</code>.</li>
</ul>
<pre>print("pi ≈ {:.2f}", 22 / 7);
print("{1} after {0}", 10, 20);
print("hex={:x}", 255);
print(abs(-3));</pre>
<p>Rational без spec друкується як <code>1/2</code>; Complex — лише через
<code>{}</code>.</p>
)HTML"},
    {"Вирази", R"HTML(
<h1>Вирази і оператори</h1>
<p>Пріоритет (зверху — слабший):</p>
<table>
<tr><td>Логіка</td><td><code>or</code>, <code>and</code>, унарний <code>not</code></td></tr>
<tr><td>Рівність</td><td><code>==</code> <code>!=</code></td></tr>
<tr><td>Порівняння</td><td><code>&lt;</code> <code>&lt;=</code> <code>&gt;</code> <code>&gt;=</code></td></tr>
<tr><td>Додавання</td><td><code>+</code> <code>-</code></td></tr>
<tr><td>Множення</td><td><code>*</code> <code>/</code> <code>%</code></td></tr>
<tr><td>Степінь</td><td><code>**</code> (права асоціативність)</td></tr>
</table>
<pre>print(2 + 3 * 4);      // 14
print(2 ** 3 ** 2);    // 512  (2**(3**2))
print(not (1 &lt; 0) and true);</pre>
<div class="warn"><code>and</code>/<code>or</code>/<code>not</code> — лише Boolean
(без неявного <code>1→true</code>). Обидва операнди <code>and</code>/<code>or</code>
обчислюються завжди (без short-circuit).</div>
)HTML"},
    {"if / else", R"HTML(
<h1>Розгалуження</h1>
<pre>if (умова) оператор
if (умова) оператор else оператор
if (умова) { ... } else if (умова) { ... } else { ... }</pre>
<ul>
<li>Умова — Boolean (інакше runtime-помилка).</li>
<li><code>else</code> чіпляється до найближчого <code>if</code>.</li>
<li>Блок <code>{ }</code> — нова область видимості для <code>let</code>.</li>
</ul>
<pre>let x = 0;
if (x &lt; 0) {
  print("neg");
} else if (x == 0) {
  print("zero");
} else {
  print("pos");
}</pre>
)HTML"},
    {"Цикли", R"HTML(
<h1>Цикли while і for</h1>
<pre>while (умова) оператор

for (init; умова; step) оператор</pre>
<ul>
<li>Умова — Boolean; порожня умова в <code>for</code> = <code>true</code>.</li>
<li><code>init</code>: порожньо, <code>let ...</code> або присвоєння;
<code>step</code>: порожньо або присвоєння.</li>
<li><code>let</code> у <code>init</code> видимий у умові, step і тілі.</li>
<li><code>break;</code> / <code>continue;</code> — на найближчий цикл.</li>
</ul>
<pre>let i = 0;
while (i &lt; 3) {
  print(i);
  i = i + 1;
}

for (let n = 0; n &lt; 3; n = n + 1) {
  if (n == 1) continue;
  print(n);
}</pre>
)HTML"},
    {"for-in", R"HTML(
<h1>Цикл for-in</h1>
<pre>for (x in arr) { ... }       // елементи масиву / ключі запису
for (i, x in arr) { ... }    // індекс і елемент
for (k, v in obj) { ... }    // ключ і значення</pre>
<ul>
<li>По запису з одним імʼям — ключі (String у ВМ); <code>obj[k]</code> працює.</li>
<li>Ітерація йде по <b>знімку</b> довжини на старті: <code>push</code>/<code>pop</code>
у тілі не змінюють кількість кроків.</li>
</ul>
<pre>let a = [10, 20, 30];
for (i, v in a) {
  print("{}:{}", i, v);
}

let p = {x: 1, y: 2};
for (k, v in p) {
  print("{}={}", k, v);
}</pre>
)HTML"},
    {"Функції", R"HTML(
<h1>Функції fun / return</h1>
<pre>fun name(a, b) {
  return a + b;
}
print(name(1, 2));</pre>
<ul>
<li>Оголошення <code>fun</code> — лише на <b>верхньому рівні</b> (не в блоках).</li>
<li>Параметри — локалі кадру; тіло має власні <code>let</code> (без замикань).</li>
<li><code>return вираз;</code> або <code>return;</code> (тоді <code>0</code>).
Без <code>return</code> — неявно <code>0</code>.</li>
<li>Взаємна рекурсія дозволена.</li>
<li>Невірна арність — помилка компіляції. Макс. 255 параметрів/аргументів.</li>
<li>Не можна назвати <code>fun</code> як builtin або вбудовану константу.</li>
</ul>
<pre>fun fact(n) {
  if (n &lt;= 1) return 1;
  return n * fact(n - 1);
}
print(fact(5));  // 120</pre>
)HTML"},
    {"Builtins", R"HTML(
<h1>Вбудовані функції</h1>
<table>
<tr><td>Тригонометрія</td><td><code>sin cos tan asin acos atan</code> (1);
<code>atan2(y,x)</code> (2, дійсні)</td></tr>
<tr><td>Гіперболічні</td><td><code>sinh cosh tanh</code></td></tr>
<tr><td>Степінь / log</td><td><code>sqrt exp ln</code> (<code>log</code>=<code>ln</code>)
<code>log10 log2</code>; <code>pow hypot</code> (2)</td></tr>
<tr><td>Модуль</td><td><code>abs sign</code> (<code>sign</code> — лише дійсні)</td></tr>
<tr><td>Комплексні</td><td><code>re im arg conj</code></td></tr>
<tr><td>Округлення</td><td><code>floor ceil round trunc</code> (дійсні)</td></tr>
<tr><td>Порівняння</td><td><code>min max</code> (2, дійсні)</td></tr>
<tr><td>Кути</td><td><code>deg rad</code></td></tr>
<tr><td>Колекції</td><td><code>len push pop</code></td></tr>
</table>
<pre>print(abs(-3));
print(hypot(3, 4));
print(floor(3.7));
print("re={} im={}", re(3 + 4i), im(3 + 4i));
print(sin(pi / 2));</pre>
<div class="tip"><code>sqrt</code> відʼємного дійсного дає Complex.
<code>abs(INT64_MIN)</code> → IntegerOverflow.</div>
)HTML"},
    {"Константи", R"HTML(
<h1>Вбудовані константи</h1>
<p>Імена без <code>()</code>. <code>let</code> з тим самим імʼям затінює константу.</p>
<table>
<tr><td>Математика</td><td><code>pi e tau phi sqrt2 sqrt3 ln2 ln10 i</code></td></tr>
<tr><td>Фізика</td><td><code>c G h hbar qe me mp mn u eps0 mu0 alpha sigma g atm Ry a0</code></td></tr>
<tr><td>Хімія</td><td><code>NA kB</code> (також <code>k</code>) <code>R F Vm celsius0</code></td></tr>
</table>
<pre>print("{:.4f}", pi);
print(c);
print(1 + i);
print(NA * qe == F);</pre>
<div class="tip">Елементарний заряд — <code>qe</code>. Імʼя <code>e</code> —
число Ейлера, не заряд.</div>
)HTML"},
    {"Масиви", R"HTML(
<h1>Масиви</h1>
<pre>let a = [1, 2, 3];
let empty = [];
print(a[0]);
a[1] = 9;
push(a, 4);
print(pop(a));
print(len(a));</pre>
<ul>
<li>Індекс — Integer з 0.</li>
<li><b>За посиланням:</b> <code>let b = a</code> ділить дані; мутації через
<code>b</code> видно в <code>a</code>.</li>
<li><code>push</code> повертає той самий масив; <code>pop</code> — знятий елемент.</li>
<li>Цикл <code>a[0] = a</code> дозволений; <code>print</code> показує <code>...</code>
на повторному заході.</li>
</ul>
)HTML"},
    {"Записи", R"HTML(
<h1>Записи (обʼєкти)</h1>
<pre>let p = {x: 1, y: 2};
let empty = {};
print(p.x);
p.z = 3;
print(len(p));</pre>
<div class="warn">Рядковий літерал <code>"…"</code> у виразах (крім формату
<code>print</code>) заборонений. Для динамічного ключа — змінна з for-in:</div>
<pre>for (k in p) {
  print(p[k]);
}
for (k, v in p) {
  print("{}={}", k, v);
}</pre>
<ul>
<li>У виразі <code>{ ... }</code> — запис; у операторі <code>{ ... }</code> — блок.</li>
<li>Семантика за посиланням, як у масивів. Методів з <code>this</code> немає.</li>
</ul>
)HTML"},
    {"Арифметика", R"HTML(
<h1>Арифметика і помилки</h1>
<ul>
<li><code>+</code> <code>-</code> <code>*</code>: Integer лишається Integer, поки не
зʼявиться Rational/Real/Complex.</li>
<li>Integer+Rational → Rational (скорочений; знаменник 1 → Integer).</li>
<li>будь-що + Complex → Complex.</li>
<li><code>%</code>: Integer→Integer (до 0); інакше Real; Complex/Boolean — помилка.</li>
<li><code>**</code> — степінь.</li>
<li>Порівняння чисел (не Complex) → Boolean; <code>==</code>/<code>!=</code> також
для Boolean.</li>
</ul>
<p>Real <b>не звужується</b> назад до Integer (<code>2.0 * 3.0</code> — Real).</p>
<p>Помилки виконання: переповнення цілого, ділення на нуль, Inf/NaN,
недопустима операція (<code>true + 1</code>, <code>1 and true</code>,
<code>2i &lt; 3i</code>).</p>
<pre>print(1/2 + 1/2);   // 1 (Rational → Integer)
print(1 / 2);       // 0.5 (Real)
print((-2) ** 3);</pre>
)HTML"},
    {"Збірка", R"HTML(
<h1>Збірка і встановлення</h1>
<p>Потрібні CMake ≥ 3.28, C++23; для IDE — ще Qt 6.12 Widgets.</p>
<pre>cmake -S . -B build-ide -DCALX_BUILD_IDE=ON \
  -DCMAKE_PREFIX_PATH=/шлях/до/Qt/6.12.0/gcc_64 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-ide -j
./build-ide/calx
./build-ide/ide/calx-ide</pre>
<p>З <code>CALX_BUILD_IDE=ON</code> збираються <b>обидва</b> бінарники.
Встановлення в <code>~/.local/bin</code> (і CLI, і IDE):</p>
<pre>cmake --install build-ide --prefix "$HOME/.local"</pre>
<p>Лише консоль (без Qt): дерево <code>build</code> без
<code>CALX_BUILD_IDE</code>, потім
<code>cmake --install build --prefix "$HOME/.local"</code>.</p>
)HTML"},
    {"Ліцензія", R"HTML(
<h1>Ліцензія</h1>
<p><b>calx</b> і <b>calx-ide</b> — GNU GPL v3 або пізніша
(<code>LICENSE</code>, <code>COPYRIGHT</code>). © 2026 Ihor Dolinskyi.</p>
<ul>
<li><b>Qt 6</b> (Widgets) — зазвичай LGPL відкритої збірки Qt.</li>
<li><b>replxx</b> — лише CLI REPL (не в calx-ide).</li>
<li><b>doctest</b> — лише тести.</li>
</ul>
<p>Подробиці — файл <code>NOTICE</code>.</p>
<div class="tip">Можна користуватись і ділитись за GPL. Зміни в поширюваній
збірці також мають бути під GPL.</div>
)HTML"},

};

} // namespace

QString HelpDialog::pageChrome(const QString &body_html) {
  return QStringLiteral(
             "<!DOCTYPE html><html><head><meta charset='utf-8'><style>"
             "body{font-family:'Noto Sans','DejaVu Sans',Ubuntu,sans-serif;"
             "font-size:16px;line-height:1.55;color:#1a1d23;background:#f4f5f7;"
             "margin:0;padding:1.1em 1.4em 2em;}"
             "h1{font-size:1.45em;color:#12151a;margin:0 0 "
             "0.6em;font-weight:700;}"
             "h2{font-size:1.12em;color:#1a2330;margin:1.2em 0 "
             "0.45em;font-weight:650;}"
             "p,li,td{color:#1a1d23;}"
             "ul,ol{padding-left:1.35em;}"
             "li{margin:0.38em 0;}"
             "table{border-collapse:collapse;margin:0.6em 0;width:100%;}"
             "td{padding:0.28em 0.9em 0.28em 0;vertical-align:top;}"
             "tr{border-bottom:1px solid #dde1e6;}"
             "pre{font-family:'Noto Sans Mono','DejaVu Sans Mono',monospace;"
             "font-size:0.92em;background:#e4e7ec;color:#12151a;"
             "padding:0.75em 1em;border-radius:6px;overflow-x:auto;}"
             "code{font-family:'Noto Sans Mono','DejaVu Sans Mono',monospace;"
             "font-size:0.92em;background:#e4e7ec;color:#12151a;"
             "padding:0.12em 0.35em;border-radius:3px;}"
             ".tip,.warn,.ok{padding:0.75em 0.95em;margin:0.95em 0;"
             "border-radius:6px;color:#1a1d23;}"
             ".tip{background:#efe6c8;border:1px solid #d4bc6a;}"
             ".warn{background:#f0d6d6;border:1px solid #d09090;}"
             ".ok{background:#d8e8d8;border:1px solid #8bbb8b;}"
             ".tip b,.warn b,.ok b{color:#12151a;}"
             "a{color:#1a5f9e;}"
             "</style></head><body>%1</body></html>")
      .arg(body_html);
}

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) { finishSetup(0); }

HelpDialog::HelpDialog(const QString &section_title, QWidget *parent)
    : QDialog(parent) {
  finishSetup(sectionIndexByTitle(section_title));
}

int HelpDialog::sectionIndexByTitle(const QString &title) {
  for (int i = 0; i < static_cast<int>(std::size(kSections)); ++i) {
    if (title ==
        QString::fromUtf8(kSections[static_cast<std::size_t>(i)].title)) {
      return i;
    }
  }
  return 0;
}

void HelpDialog::finishSetup(int initial_row) {
  setWindowTitle(QStringLiteral("Довідка calx IDE"));
  resize(1000, 720);
  setMinimumSize(720, 480);

  QPalette dlg = palette();
  dlg.setColor(QPalette::Window, QColor(0x2a2f38));
  dlg.setColor(QPalette::WindowText, QColor(0xe8eaed));
  dlg.setColor(QPalette::Base, QColor(0xf4f5f7));
  dlg.setColor(QPalette::Text, QColor(0x1a1d23));
  dlg.setColor(QPalette::Button, QColor(0x3a424e));
  dlg.setColor(QPalette::ButtonText, QColor(0xe8eaed));
  dlg.setColor(QPalette::Highlight, QColor(0x3d7ea6));
  dlg.setColor(QPalette::HighlightedText, QColor(0xffffff));
  setPalette(dlg);

  auto *title = new QLabel(QStringLiteral("Довідка"), this);
  {
    QFont f = title->font();
    f.setPointSize(qMax(12, f.pointSize() + 2));
    f.setBold(true);
    title->setFont(f);
  }
  title->setStyleSheet(
      QStringLiteral("color:#e8eaed; padding: 4px 2px 8px 2px;"));

  nav_ = new QListWidget(this);
  nav_->setFixedWidth(200);
  nav_->setStyleSheet(
      QStringLiteral("QListWidget {"
                     "  background:#232830; color:#d4d8de; border:none;"
                     "  outline:none; font-size:13px; padding:6px 0;"
                     "}"
                     "QListWidget::item {"
                     "  padding:8px 12px; margin:1px 6px; border-radius:6px;"
                     "}"
                     "QListWidget::item:selected {"
                     "  background:#3d7ea6; color:#ffffff;"
                     "}"
                     "QListWidget::item:hover:!selected {"
                     "  background:#2f3640;"
                     "}"));
  for (const HelpSection &section : kSections) {
    nav_->addItem(QString::fromUtf8(section.title));
  }

  browser_ = new QTextBrowser(this);
  browser_->setOpenExternalLinks(true);
  browser_->setFrameShape(QFrame::NoFrame);
  {
    QPalette pal = browser_->palette();
    pal.setColor(QPalette::Base, QColor(0xf4f5f7));
    pal.setColor(QPalette::Text, QColor(0x1a1d23));
    pal.setColor(QPalette::WindowText, QColor(0x1a1d23));
    pal.setColor(QPalette::Link, QColor(0x1a5f9e));
    browser_->setPalette(pal);
    QFont font = browser_->font();
    font.setPointSize(qMax(11, font.pointSize() + 1));
    browser_->setFont(font);
  }

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрити"));
  buttons->setStyleSheet(
      QStringLiteral("QPushButton { background:#3a424e; color:#e8eaed; "
                     "border:1px solid #505868;"
                     "  padding:6px 16px; border-radius:4px; }"
                     "QPushButton:hover { background:#4a5462; }"));

  connect(nav_, &QListWidget::currentRowChanged, this,
          &HelpDialog::showSection);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

  auto *left = new QWidget(this);
  auto *left_layout = new QVBoxLayout(left);
  left_layout->setContentsMargins(10, 10, 4, 10);
  left_layout->setSpacing(4);
  left_layout->addWidget(title);
  left_layout->addWidget(nav_, 1);

  auto *right = new QWidget(this);
  auto *right_layout = new QVBoxLayout(right);
  right_layout->setContentsMargins(4, 10, 10, 10);
  right_layout->setSpacing(8);
  right_layout->addWidget(browser_, 1);
  right_layout->addWidget(buttons, 0, Qt::AlignRight);

  auto *split = new QSplitter(Qt::Horizontal, this);
  split->addWidget(left);
  split->addWidget(right);
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  split->setChildrenCollapsible(false);
  split->setStyleSheet(
      QStringLiteral("QSplitter::handle { background:#1a1e24; width:2px; }"));

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->addWidget(split);

  const int row = std::clamp(initial_row, 0, std::max(0, nav_->count() - 1));
  nav_->setCurrentRow(row);
}

void HelpDialog::showSection(int index) {
  if (index < 0 || index >= static_cast<int>(std::size(kSections))) {
    return;
  }
  const HelpSection &section = kSections[static_cast<std::size_t>(index)];
  browser_->setHtml(pageChrome(QString::fromUtf8(section.html)));
  browser_->moveCursor(QTextCursor::Start);
}
