#include "help/HelpDialog.h"

#include "calc/builtin.hpp"
#include "calc/constants.hpp"

#include <QCloseEvent>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QSplitter>
#include <QTextDocument>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <string_view>

#include <QtMath>

#include <algorithm>
#include <iterator>

namespace {

struct HelpSection {
  const char *id;
  const char *title;
  const char *html;
};

constexpr HelpSection kSections[] = {
    {"overview", "Огляд", R"HTML(
<p class="lead"><b>calx-ide</b> — редактор, калькулятор і дебаг для мови
<b>calx</b> в одному вікні.</p>

<h1>Що таке calx IDE</h1>
<p>У calx зручно рахувати з чіткими типами: цілі, дроби, дійсні, комплексні,
плюс масиви й записи, свої <code>fun</code> і математичні builtins. IDE дає
редактор файлів <code>.calc</code>, інтерактив і дебаг на тій самій ВМ, що й
консольна програма <code>calx</code>.</p>

<h2>Два режими</h2>
<ol>
<li><b>Редактор</b> — писати й зберігати програми, запускати їх цілком,
ставити точки зупину, форматувати код.</li>
<li><b>Інтерактив</b> — вводити вирази й команди по одному; змінні та
<code>fun</code> накопичуються в сесії між рядками.</li>
</ol>
<p>Перемикання — меню <a href="help:panels">Вигляд</a> або кнопки на панелі інструментів.
Довідку (це вікно) можна тримати відкритою поряд із кодом: вона <b>не блокує</b> IDE.</p>

<h2>Структура вікна</h2>
<ul>
<li>Центр — редактор або REPL.</li>
<li>Знизу — <a href="help:panels">Консоль</a> (вивід <code>print</code> і помилки).</li>
<li>Праворуч — <a href="help:panels">Змінні</a>, Функції, Константи, Дізасемблер.</li>
<li>Зверху — меню й панель запуску / дебагу.</li>
</ul>

<div class="callout tip"><div class="callout-title">Порада</div><p>Консольний <code>calx</code> — окрема програма з тією ж мовою. Історія інтерактиву спільна: <code>~/.calx_history</code>. Почніть із розділу <a href="help:start">З чого почати</a>.</p></div>

<div class="callout ok"><div class="callout-title">На замітку</div><p>Натисніть <b>F1</b> будь-коли, щоб знову відкрити або підняти довідку.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:start">Старт</a> · <a href="help:editor">Редактор</a> · <a href="help:builtins">Builtins</a></nav><nav class="pager"><span></span> <a class="navnext" href="help:start">З чого почати →</a></nav>
)HTML"},
    {"start", "З чого почати", R"HTML(
<p class="lead">Короткий шлях від порожнього вікна до першого результату.</p>
<h1>З чого почати</h1>

<h2>1. Перша програма в редакторі</h2>
<p>У режимі <b>Редактор</b> наберіть або залиште приклад за замовчуванням:</p>
<pre>let x = 2 + 2;
print("x = {}", x);</pre>
<p>Натисніть <b>Ctrl+R</b> (меню Виконання → Запустити). У панелі
<a href="help:panels">Консоль</a> з’явиться <code>x = 4</code>, у
<a href="help:panels">Змінні</a> — імʼя <code>x</code> з типом і значенням.</p>

<h2>2. Інтерактив</h2>
<p>Перемкніться на <b>Інтерактив</b> і введіть щось подібне біля підказки
<code>calx&gt;</code>:</p>
<pre>calx&gt; let a = 12 * (3 + 2)
calx&gt; print("sum = {}", a + -1.23)
sum = 58.77</pre>
<p>Детальніше про багаторядковий ввід і історію — у розділі
<a href="help:repl">Інтерактив</a>.</p>

<h2>3. Форматування й довідка</h2>
<ul>
<li><b>Ctrl+I</b> — <a href="help:format">форматувати</a> код у редакторі.</li>
<li>Опційно: Редагування → Форматувати при збереженні.</li>
<li><b>F1</b> — ця довідка.</li>
</ul>

<h2>Що вивчати далі</h2>
<ol>
<li><a href="help:types">Типи і літерали</a> — <code>1/2</code> проти <code>1 / 2</code>.</li>
<li><a href="help:vars">Змінні</a>, <a href="help:print">print</a>, <a href="help:casts">приведення й is_*</a>.</li>
<li><a href="help:if">if / else</a>, <a href="help:loops">цикли</a>, <a href="help:forin">for-in</a>, <a href="help:functions">функції</a>.</li>
<li><a href="help:arrays">Масиви</a> і <a href="help:records">записи</a> (<code>clone</code>, <code>same</code>, <code>has</code>).</li>
<li><a href="help:builtins">Builtins</a> і <a href="help:constants">константи</a>.</li>
<li><a href="help:debug">Дебаг</a> — коли програма більша.</li>
</ol>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:keys">Клавіші</a> · <a href="help:types">Типи</a></nav><nav class="pager"><a class="navprev" href="help:overview">← Огляд</a> <a class="navnext" href="help:editor">Редактор →</a></nav>
)HTML"},
    {"editor", "Редактор", R"HTML(
<p class="lead">Файли <code>.calc</code>: підсвітка, доповнення, точки зупину,
формат.</p>
<h1>Режим редактора</h1>

<h2>Файли</h2>
<ul>
<li><b>Файл → Новий / Відкрити / Зберегти / Зберегти як</b> — стандартна робота з диском.</li>
<li>Розширення за замовчуванням — <code>.calc</code>.</li>
<li>Незбережені зміни IDE питає при виході або відкритті іншого файлу.</li>
</ul>

<h2>Під час набору</h2>
<ul>
<li><b>Підсвітка</b> — ключові слова, числа, рядки формату, builtins, константи, коментарі <code>//</code>.</li>
<li><b>Доповнення</b> — під час введення ідентифікатора або <b>Ctrl+Space</b>.</li>
<li><b>Hover</b> — наведіть курсор на builtin / константу / ключове слово:
зʼявиться коротка підказка.</li>
<li><b>Tab</b> — два пробіли; <b>Enter</b> — автовідступ за незакритими <code>{ ( [</code>.</li>
</ul>

<h2>Форматування</h2>
<p><b>Ctrl+I</b> вирівнює відступи й пробіли (див. <a href="help:format">Форматування</a>).
Галочка <b>Форматувати при збереженні</b> запамʼятовується між запусками.
Якщо розбір не вдався — текст не змінюється; при автозбереженні IDE запитає,
чи зберегти «як є».</p>

<h2>Помилки в тексті</h2>
<p>Помилки компіляції й виконання підкреслюються хвилястою лінією; повне
повідомлення — у <a href="help:panels">Консолі</a> у форматі
<code>помилка:рядок:стовпець: …</code>.</p>

<div class="callout ok"><div class="callout-title">На замітку</div><p>Клік у жолобі ліворуч від номера рядка або <b>F9</b> — <a href="help:debug">точка зупину</a>.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:format">Формат</a> · <a href="help:keys">Клавіші</a> · <a href="help:debug">Дебаг</a></nav><nav class="pager"><a class="navprev" href="help:start">← Старт</a> <a class="navnext" href="help:repl">Інтерактив →</a></nav>
)HTML"},
    {"repl", "Інтерактив", R"HTML(
<p class="lead">Інтерактив: змінні й <code>fun</code> лишаються між рядками.</p>
<h1>Інтерактив (REPL)</h1>

<p>Успішно виконані рядки <b>дописуються до сесії</b>. Наступний рядок бачить
усі попередні <code>let</code> і <code>fun</code>. Це зручно для досліджень
і швидких перевірок без збереження файлу.</p>

<h2>Ввід</h2>
<ul>
<li>Ввід і результат — в одному вікні. Друкуєте після <code>calx&gt;</code>,
вище лишається хід сесії.</li>
<li><b>Enter</b> — виконати поточний ввід (якщо дужки збалансовані).</li>
<li><b>Shift+Enter</b> — новий рядок без виконання.</li>
<li>Незакриті <code>{ ( [</code> — наступний промпт <code>...&gt;</code>.</li>
<li><b>↑ / ↓</b> — історія з файлу <code>~/.calx_history</code> (спільна з CLI
<code>calx</code>).</li>
</ul>

<h2>Команди REPL</h2>
<table>
<tr><td><code>:help</code></td><td>коротка підказка в консолі REPL</td></tr>
<tr><td><code>:clear</code></td><td>скинути накопичену сесію (змінні й fun зникають)</td></tr>
</table>

<div class="callout warn"><div class="callout-title">Увага</div><p>У REPL крапку з комою в кінці простого виразу можна опустити. Після блоків <code>fun</code> / <code>if</code> / <code>while</code> / <code>for</code> вона теж не потрібна. У <b>файлах</b> редактора <code>;</code> після операторів обовʼязкова.</p></div>

<h2>Приклад сесії</h2>
<pre>calx&gt; let a = 12 * (3 + 2)
calx&gt; print("sum = {}", a + -1.23)
sum = 58.77
calx&gt; fun add(x, y) {
...&gt;   return x + y;
...&gt; }
calx&gt; print(add(2, 3))
5</pre>

<p>Панелі <a href="help:panels">Змінні</a> / Функції оновлюються після успішного рядка —
як після запуску файлу.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:vars">Змінні</a> · <a href="help:functions">Функції</a></nav><nav class="pager"><a class="navprev" href="help:editor">← Редактор</a> <a class="navnext" href="help:debug">Дебаг →</a></nav>
)HTML"},
    {"debug", "Дебаг", R"HTML(
<p class="lead">Крок за кроком з редактора: пауза на рядках і огляд стану.</p>
<h1>Дебаг</h1>

<ol>
<li>Поставте <b>breakpoints</b>: клік у gutter або курсор на рядку + <b>F9</b>.</li>
<li><b>Shift+F5</b> — почати дебаг (пауза на старті / перед виконанням).</li>
<li><b>F5</b> — продовжити до наступної точки зупину або кінця програми.</li>
<li><b>F11</b> — крок усередину; <b>F10</b> — крок через; <b>Shift+F11</b> — крок назовні.</li>
<li><b>Пауза</b> — запропонувати зупинитись ASAP; <b>Стоп</b> — завершити сесію дебагу.</li>
</ol>

<p>На паузі підсвічується поточний рядок джерела. У панелях видно локальні
змінні поточного кадру й <a href="help:panels">дізасемблер</a> байткоду. Кнопки
Continue / Step тимчасово вимикаються, поки виконується довгий Continue
(індикатор «зайнятості»).</p>

<div class="callout warn"><div class="callout-title">Увага</div><p>Точки зупину привʼязані до номерів рядків <b>джерела в Редакторі</b>. Дебаг не підключається до окремої REPL-сесії — спочатку перемкніться в редактор і запустіть дебаг для тексту файлу.</p></div>

<h2>Типовий сценарій</h2>
<pre>fun fact(n) {
  if (n <= 1) return 1;
  return n * fact(n - 1);
}
print(fact(5));</pre>
<p>Поставте breakpoint на <code>return n * fact(n - 1);</code>, запустіть
дебаг і дивіться, як змінюється <code>n</code> у панелі змінних на кожній
паузі.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:keys">Клавіші</a> · <a href="help:functions">Функції</a></nav><nav class="pager"><a class="navprev" href="help:repl">← Інтерактив</a> <a class="navnext" href="help:panels">Панелі →</a></nav>
)HTML"},
    {"panels", "Панелі", R"HTML(
<p class="lead">Панелі: вивід, змінні, функції — після запуску або на паузі
дебагу.</p>
<h1>Панелі</h1>

<table>
<tr><td><b>Консоль</b></td><td>Вивід <code>print</code> і повідомлення про помилки
компіляції / виконання.</td></tr>
<tr><td><b>Змінні</b></td><td>Імʼя, тип і значення локалей після Run або на паузі.</td></tr>
<tr><td><b>Функції</b></td><td>Ваші <code>fun</code>: імʼя та кількість параметрів.</td></tr>
<tr><td><b>Константи</b></td><td>Вбудовані математичні й фізичні константи
(див. також <a href="help:constants">розділ Константи</a>).</td></tr>
<tr><td><b>Дізасемблер</b></td><td>Внутрішній байткод chunk — для діагностики й цікавості.</td></tr>
</table>

<p>Показати чи сховати — меню <b>Вигляд</b>. Панелі можна перетягувати й
зʼєднувати у вкладки мишею. Тема світла/темна теж у Вигляді.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:editor">Редактор</a> · <a href="help:constants">Константи</a></nav><nav class="pager"><a class="navprev" href="help:debug">← Дебаг</a> <a class="navnext" href="help:keys">Клавіші →</a></nav>
)HTML"},
    {"keys", "Гарячі клавіші", R"HTML(
<p class="lead">Гарячі клавіші: запуск, дебаг, формат, довідка.</p>
<h1>Гарячі клавіші</h1>

<table class="keys">
<tr><td><kbd>F1</kbd></td><td>Довідка (відкрити / підняти вікно)</td></tr>
<tr><td><kbd>Ctrl+R</kbd></td><td>Запустити програму з редактора</td></tr>
<tr><td><kbd>F5</kbd></td><td>Продовжити дебаг; якщо дебагу немає — запуск</td></tr>
<tr><td><kbd>Shift+F5</kbd></td><td>Почати дебаг</td></tr>
<tr><td><kbd>F9</kbd></td><td>Точка зупину на поточному рядку</td></tr>
<tr><td><kbd>F10</kbd> / <kbd>F11</kbd></td><td>Крок через / усередину</td></tr>
<tr><td><kbd>Shift+F11</kbd></td><td>Крок назовні</td></tr>
<tr><td><kbd>Ctrl+I</kbd></td><td>Форматувати</td></tr>
<tr><td><kbd>Ctrl+Space</kbd></td><td>Доповнення</td></tr>
<tr><td><kbd>Ctrl+N / O / S</kbd></td><td>Новий / Відкрити / Зберегти</td></tr>
</table>

<p>В <a href="help:repl">інтерактиві</a>: <kbd>Enter</kbd> — виконати,
<kbd>Shift+Enter</kbd> — новий рядок, <kbd>↑</kbd><kbd>↓</kbd> — історія.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:debug">Дебаг</a> · <a href="help:editor">Редактор</a></nav><nav class="pager"><a class="navprev" href="help:panels">← Панелі</a> <a class="navnext" href="help:format">Форматування →</a></nav>
)HTML"},
    {"format", "Форматування", R"HTML(
<p class="lead">Вирівнює відступи й пробіли; сенс коду не змінює.</p>
<h1>Форматування коду</h1>

<p><b>Ctrl+I</b> (або автомат при збереженні) переписує AST назад у текст:</p>
<ul>
<li>відступи по 2 пробіли;</li>
<li>пробіли навколо бінарних операторів (<code>a + b</code>, <code>x == 1</code>);</li>
<li>коми в списках аргументів, масивах і записах;</li>
<li>ланцюжки <code>else if</code> в одному стилі;</li>
<li>коментарі <code>//</code> зберігаються на своїх місцях наскільки можливо.</li>
</ul>

<pre>// було
let x=1+2*3;if(x&gt;0){print(x);}

// стало
let x = 1 + 2 * 3;
if (x &gt; 0) {
  print(x);
}</pre>

<div class="callout warn"><div class="callout-title">Увага</div><p>Якщо є синтаксична помилка — форматування <b>не змінює</b> файл. Спочатку виправте підкреслене місце або повідомлення в Консолі.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:editor">Редактор</a></nav><nav class="pager"><a class="navprev" href="help:keys">← Клавіші</a> <a class="navnext" href="help:types">Типи →</a></nav>
)HTML"},
    {"types", "Типи і літерали", R"HTML(
<p class="lead">Тип береться з літерала чи операції — окремо його не пишуть.</p>
<h1>Типи і літерали</h1>

<table>
<tr><td><b>Boolean</b></td><td><code>true</code>, <code>false</code></td>
<td>Єдиний тип для умов <code>if</code> / <code>while</code> і логіки.</td></tr>
<tr><td><b>Integer</b></td><td><code>12</code>, <code>-3</code></td>
<td>Ціле зі знаком (64-бітне).</td></tr>
<tr><td><b>Rational</b></td><td><code>1/2</code>, <code>3/4</code></td>
<td><b>Без</b> пробілів навколо <code>/</code> — це літерал дробу.</td></tr>
<tr><td><b>Real</b></td><td><code>2.0</code>, <code>.5</code>, <code>2e1</code></td>
<td>Дійсне (long double у реалізації).</td></tr>
<tr><td><b>Complex</b></td><td><code>2i</code>, <code>2 + 3i</code></td>
<td>Дійсна + уявна частина.</td></tr>
<tr><td><b>Array</b></td><td><code>[1, 2, 3]</code>, <code>[]</code></td>
<td>Список значень; див. <a href="help:arrays">Масиви</a>.</td></tr>
<tr><td><b>Record</b></td><td><code>{x: 1, y: 2}</code>, <code>{}</code></td>
<td>Іменовані поля; див. <a href="help:records">Записи</a>.</td></tr>
</table>

<div class="callout tip"><div class="callout-title">Порада</div><p><code>1/2</code> — дріб (Rational). <code>1 / 2</code> — операція ділення → Real <code>0.5</code>. Пробіл змінює сенс!</p></div>

<pre>let q = 3/4;
let z = 1 + 2i;
print(q);   // 3/4
print(z);   // 1 + 2i
print(1 / 2);  // 0.5</pre>

<p>Коментар: <code>//</code> до кінця рядка. Рядковий літерал <code>"…"</code>
у виразах писати не можна (лише формат у <a href="help:print">print</a>).
Ключі з <a href="help:forin">for-in</a> по запису — це String у ВМ; їх можна
класти в масив і перевіряти через <code>is_string</code>.</p>

<p>Явне приведення — <a href="help:casts">int, real, bool, complex, rational</a>.
Перевірка типу значення — <code>is_int</code>, <code>is_array</code>, …
(див. <a href="help:builtins">Builtins</a> і розділ нижче в
<a href="help:casts">Приведенні</a>).</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:arith">Арифметика</a> · <a href="help:constants">Константи</a></nav><nav class="pager"><a class="navprev" href="help:format">← Формат</a> <a class="navnext" href="help:casts">Приведення →</a></nav>
)HTML"},
    {"casts", "Приведення типів", R"HTML(
<p class="lead">Явне приведення типів і перевірка <code>is_*</code>. Умови не
приймають число «за правдивість» самі по собі.</p>
<h1>Явне приведення типів</h1>

<p>Умови <code>if</code> / <code>while</code> і оператори <code>and</code> /
<code>or</code> / <code>not</code> працюють <b>лише</b> з Boolean. Число
<code>n</code> у <code>if (n)</code> — помилка. Щоб перевірити «чи не нуль»:</p>
<pre>if (bool(n)) { print("так"); }</pre>
<p>або порівняння: <code>if (n != 0)</code>.</p>

<table>
<tr><td><code>int(x)</code></td><td>Число → Integer (обрізання до нуля).
Rational — лише якщо це вже ціле (<code>den == 1</code>). Complex / Boolean /
колекції — помилка. Поза діапазоном цілого, Inf або NaN —
<code>IntegerOverflow</code>.</td></tr>
<tr><td><code>real(x)</code></td><td>Integer / Rational / Real → Real.
Complex — помилка.</td></tr>
<tr><td><code>bool(x)</code></td><td>Boolean як є. Число: лише нуль →
<code>false</code>, інакше <code>true</code> (у т.ч. Inf). NaN —
<code>InvalidOperation</code>. Complex / масив / запис — помилка.</td></tr>
<tr><td><code>complex(re)</code></td><td><code>re + 0i</code></td></tr>
<tr><td><code>complex(re, im)</code></td><td>комплексне з двох дійсних чисел</td></tr>
<tr><td><code>rational(n, d)</code></td><td>два Integer → Rational (скорочення дробу)</td></tr>
</table>

<pre>print(int(3.9));          // 3
print(int(-3.9));         // -3
print(real(1/2));         // 0.5
print(bool(0));           // false
print(bool(2));           // true
print(complex(2));        // 2 + 0i
print(complex(2, 3));     // 2 + 3i
print(rational(2, 4));    // 1/2</pre>

<h2 id="istype">Перевірка типу: is_*</h2>
<p>У змішаному масиві чи полі запису спочатку перевірте тип.
Кожна з цих функцій бере одне значення і повертає Boolean:</p>
<table>
<tr><td><code>is_bool(x)</code></td><td>Boolean</td></tr>
<tr><td><code>is_int(x)</code></td><td>Integer</td></tr>
<tr><td><code>is_rational(x)</code></td><td>Rational</td></tr>
<tr><td><code>is_real(x)</code></td><td>Real</td></tr>
<tr><td><code>is_complex(x)</code></td><td>Complex</td></tr>
<tr><td><code>is_number(x)</code></td><td>Integer, Rational, Real або Complex</td></tr>
<tr><td><code>is_array(x)</code></td><td>Array</td></tr>
<tr><td><code>is_record(x)</code></td><td>Record</td></tr>
<tr><td><code>is_string(x)</code></td><td>ключ з <code>for (k in record)</code></td></tr>
</table>
<pre>let mixed = [1, true, {x: 2}];
for (v in mixed) {
  if (is_int(v)) { print(v + 1); }
  if (is_bool(v)) { print(v); }
  if (is_record(v) and (v has x)) { print(v.x); }
}</pre>
<p>Це <b>не</b> приведення: <code>is_int(2.0)</code> → <code>false</code>
(це Real). Щоб отримати Integer — <code>int(...)</code>.</p>

<div class="callout tip"><div class="callout-title">Порада</div><p>Округлення «як у математиці» — спочатку <a href="help:builtins">floor / round / trunc</a>, потім за потреби <code>int(...)</code>: <code>int(floor(3.7))</code> → 3.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:builtins">Builtins</a> · <a href="help:if">if / else</a> · <a href="help:arrays">Масиви</a></nav><nav class="pager"><a class="navprev" href="help:types">← Типи</a> <a class="navnext" href="help:vars">Змінні →</a></nav>
)HTML"},
    {"vars", "Змінні", R"HTML(
<p class="lead"><code>let</code> оголошує змінну; далі змінюєте її присвоєнням.</p>
<h1>Змінні і присвоєння</h1>

<pre>let name = вираз;
let a = 1, b = a * 2, c = -3.5;
name = новий_вираз;</pre>

<ul>
<li>Ініціалізатор <b>обовʼязковий</b> (<code>let a;</code> — помилка).</li>
<li>Повторне імʼя в тій самій області — помилка переоголошення.</li>
<li>Імʼя видиме лише <b>після</b> свого ініціалізатора в списку:
<code>let a = 2, b = a * 3</code> — ок; <code>let a = a + 1</code> — ні.</li>
<li>Імена: літери, цифри, <code>_</code>; не починаються з цифри.</li>
<li>Присвоєння — лише вже оголошеній змінній, елементу <code>a[i]</code>
або полю <code>rec.field</code>.</li>
</ul>

<pre>let i = 0;
i = i + 1;
print(i);  // 1</pre>

<p>Блоки <code>{ }</code> у <code>if</code> / циклах створюють вкладені області
видимості для нових <code>let</code>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:arrays">Масиви</a> · <a href="help:records">Записи</a></nav><nav class="pager"><a class="navprev" href="help:casts">← Приведення</a> <a class="navnext" href="help:print">print →</a></nav>
)HTML"},
    {"print", "Вивід print", R"HTML(
<p class="lead">Вивід у консоль — лише через <code>print</code>.</p>
<h1>Вивід: print</h1>

<pre>print(вираз);
print("формат", аргументи...);</pre>

<ul>
<li>Без рядка формату — друкує значення й переводить рядок.</li>
<li>З форматом: плейсхолдери <code>{}</code>, <code>{0}</code>,
<code>{:.2f}</code>, <code>{:d}</code>, <code>{:x}</code>.</li>
<li>Літеральні дужки в тексті — подвоїти: <code>{{</code> і <code>}}</code>.</li>
<li>Не змішуйте автоматичну (<code>{}</code>) і ручну (<code>{0}</code>)
індексацію в одному рядку формату.</li>
<li>У рядку: <code>\n</code> <code>\t</code> <code>\r</code> <code>\\</code>
<code>\"</code>.</li>
</ul>

<pre>print("pi ≈ {:.4f}", pi);
print("{1} після {0}", 10, 20);
print("hex={:x}", 255);</pre>

<p>Повний список математичних функцій для аргументів — у
<a href="help:builtins">Builtins</a>; константи на кшталт <code>pi</code> —
<a href="help:constants">Константи</a>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:types">Типи</a> · <a href="help:constants">Константи</a></nav><nav class="pager"><a class="navprev" href="help:vars">← Змінні</a> <a class="navnext" href="help:expr">Вирази →</a></nav>
)HTML"},
    {"expr", "Вирази", R"HTML(
<p class="lead">Оператори з фіксованим пріоритетом: арифметика, порівняння,
логіка, <code>has</code>.</p>
<h1>Вирази і оператори</h1>

<p>Пріоритет (зверху — <b>слабший</b>, виконується пізніше):</p>
<table>
<tr><td>Логіка</td><td><code>or</code>, потім <code>and</code>, унарний <code>not</code></td></tr>
<tr><td>Наявність поля</td><td><code>record has field</code> (field — ідентифікатор)</td></tr>
<tr><td>Рівність</td><td><code>==</code> <code>!=</code></td></tr>
<tr><td>Порівняння</td><td><code>&lt;</code> <code>&lt;=</code> <code>&gt;</code> <code>&gt;=</code></td></tr>
<tr><td>Додавання</td><td><code>+</code> <code>-</code></td></tr>
<tr><td>Множення</td><td><code>*</code> <code>/</code> <code>%</code></td></tr>
<tr><td>Степінь</td><td><code>**</code> (асоціативність <b>справа наліво</b>:
<code>2**3**2</code> = 512)</td></tr>
</table>

<pre>print(2 + 3 * 4);           // 14
print((2 + 3) * 4);         // 20
print(not false and true);  // true
print(5 % 2);               // 1
print(2 ** 3 ** 2);         // 512
let p = {x: 1};
print(p has x and true);    // true</pre>

<div class="callout warn"><div class="callout-title">Увага</div><p><code>and</code> / <code>or</code> / <code>not</code> — лише Boolean.
Обидва операнди <code>and</code>/<code>or</code> рахуються завжди (короткого
замикання немає). Число спочатку зведіть порівнянням або
<a href="help:casts">bool(...)</a>.</p></div>

<p>Підвищення типів у арифметиці — у розділі <a href="help:arith">Арифметика</a>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:casts">Приведення</a> · <a href="help:arith">Арифметика</a></nav><nav class="pager"><a class="navprev" href="help:print">← print</a> <a class="navnext" href="help:if">if / else →</a></nav>
)HTML"},
    {"if", "if / else", R"HTML(
<p class="lead"><code>if</code> / <code>else</code>: умова лише Boolean.</p>
<h1>Розгалуження</h1>

<pre>if (умова) { ... }
if (умова) { ... } else { ... }
if (умова) { ... } else if (умова) { ... } else { ... }</pre>

<ul>
<li>Умова має бути Boolean: порівняння, <code>true</code>/<code>false</code>,
результат логічного оператора або <a href="help:casts">bool(...)</a>.</li>
<li>Блок <code>{ }</code> — нова область для <code>let</code>.</li>
<li><code>else if</code> можна ланцюжити як завгодно довго.</li>
</ul>

<pre>let x = 0;
if (x < 0) {
  print("відʼємне");
} else if (x == 0) {
  print("нуль");
} else {
  print("додатне");
}</pre>

<div class="callout warn"><div class="callout-title">Увага</div><p><code>if (1)</code> або <code>if (1.0)</code> — помилка типу. Пишіть <code>if (1 != 0)</code> або <code>if (bool(1))</code>.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:loops">Цикли</a> · <a href="help:casts">bool</a></nav><nav class="pager"><a class="navprev" href="help:expr">← Вирази</a> <a class="navnext" href="help:loops">Цикли →</a></nav>
)HTML"},
    {"loops", "Цикли", R"HTML(
<p class="lead">Цикли: <code>while</code> і звичайний <code>for</code>.</p>
<h1>Цикли while і for</h1>

<pre>while (умова) { ... }

for (let i = 0; i < 3; i = i + 1) { ... }</pre>

<ul>
<li>Умова — Boolean; порожня умова в <code>for</code> означає «завжди»
(потрібен <code>break</code>).</li>
<li><code>break;</code> — негайно вийти з найближчого циклу.</li>
<li><code>continue;</code> — перейти до наступної ітерації; у <code>for</code>
спочатку виконається step.</li>
</ul>

<pre>let i = 0;
while (i < 3) {
  print(i);
  i = i + 1;
}

for (let n = 0; n < 3; n = n + 1) {
  if (n == 1) continue;
  print(n);  // 0 потім 2
}</pre>

<p>Обхід масивів і записів зручніше через <a href="help:forin">for-in</a>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:forin">for-in</a> · <a href="help:arrays">Масиви</a></nav><nav class="pager"><a class="navprev" href="help:if">← if / else</a> <a class="navnext" href="help:forin">for-in →</a></nav>
)HTML"},
    {"forin", "for-in", R"HTML(
<p class="lead">Ітерація елементів масиву або ключів / пар запису.
Детальні правила колекцій — у <a href="help:arrays">Масиви</a> та
<a href="help:records">Записи</a>.</p>
<h1>Цикл for-in</h1>

<pre>for (x in arr) { ... }       // елементи масиву або ключі запису
for (i, x in arr) { ... }    // індекс і елемент
for (k, v in obj) { ... }    // ключ і значення запису</pre>

<h2>Масив</h2>
<pre>let a = [10, 20, 30];
for (i, v in a) {
  print("{}:{}", i, v);  // 0:10, 1:20, 2:30
}

let s = 0;
for (x in a) {
  s = s + x;
}
print(s);  // 60</pre>

<h2>Запис</h2>
<pre>let p = {x: 1, y: 2};
for (k, v in p) {
  print("{}={}", k, v);
}
// ключ потім зручно для p[k]:
for (k in p) {
  print(p[k]);
}</pre>

<h2>Порожній і break</h2>
<pre>let n = 0;
for (x in []) { n = n + 1; }
print(n);  // 0

for (x in [1, 2, 3]) {
  if (x == 2) break;
  print(x);  // лише 1
}</pre>

<div class="callout tip"><div class="callout-title">Порада</div><p>І масив, і запис ітеруються зі <b>знімка</b> на старті: <code>push</code>
у тілі подовжує масив, а нове поле в записі зʼявляється — але нових кроків
<b>цього</b> циклу не буде. Приклад для масиву — у
<a href="help:arrays">Масиви → Обхід</a>.</p></div>

<div class="callout warn"><div class="callout-title">Увага</div><p><code>for (x in 1)</code> або будь-який не масив/запис — помилка типу.
Рядків і окремого «діапазону» <code>1..n</code> немає: зробіть масив або
лічильник у звичайному <a href="help:loops">for</a>.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:arrays">Масиви</a> · <a href="help:records">Записи</a> · <a href="help:loops">Цикли</a></nav><nav class="pager"><a class="navprev" href="help:loops">← Цикли</a> <a class="navnext" href="help:functions">Функції →</a></nav>
)HTML"},
    {"functions", "Функції", R"HTML(
<p class="lead">Свої функції: <code>fun</code>, параметри, <code>return</code>.</p>
<h1>Функції fun / return</h1>

<pre>fun add(a, b) {
  return a + b;
}
print(add(2, 3));  // 5</pre>

<ul>
<li><code>fun</code> — лише на <b>верхньому рівні</b> файлу (не всередині
<code>if</code> / циклів / інших fun).</li>
<li><code>return вираз;</code> або <code>return;</code> (тоді результат
<code>0</code>). Без <code>return</code> в кінці — теж <code>0</code>.</li>
<li>Рекурсія дозволена.</li>
<li>Не можна назвати функцію так само, як builtin або вбудовану константу
(<code>sin</code>, <code>pi</code>, …).</li>
</ul>

<pre>fun fact(n) {
  if (n <= 1) return 1;
  return n * fact(n - 1);
}
print(fact(5));  // 120</pre>

<p>Готові математичні функції — <a href="help:builtins">Builtins</a>. Для відлагодження
рекурсії зручний <a href="help:debug">дебаг</a>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:builtins">Builtins</a> · <a href="help:debug">Дебаг</a></nav><nav class="pager"><a class="navprev" href="help:forin">← for-in</a> <a class="navnext" href="help:builtins">Builtins →</a></nav>
)HTML"},
    {"builtins", "Builtins", R"HTML(
<p class="lead">Усі вбудовані функції: сигнатури, типи, приклади.</p>
<h1>Вбудовані функції</h1>

<p>Builtins викликаються як звичайні функції. Імена зарезервовані: власний
<code>fun sin(...)</code> оголосити не можна. Кути тригонометрії — у
<b>радіанах</b> (див. <code>deg</code> / <code>rad</code>).</p>

<nav class="toc">
<div class="toc-title">Зміст розділу</div>
<ul>
<li><a href="#trig">Тригонометрія</a></li>
<li><a href="#hyper">Гіперболічні</a></li>
<li><a href="#powlog">Корінь, exp, логарифми, степінь</a></li>
<li><a href="#abs">Модуль і знак</a></li>
<li><a href="#cplx">Комплексні</a></li>
<li><a href="#round">Округлення</a></li>
<li><a href="#minmax">min / max</a></li>
<li><a href="#angles">Кути deg / rad</a></li>
<li><a href="#coll">Колекції</a></li>
<li><a href="#castfn">Приведення типів</a></li>
<li><a href="#istypefn">Перевірка типу</a></li>
</ul>
</nav>

<h2 id="trig">Тригонометрія</h2>
<p>Аргументи — числові (Integer / Rational / Real / Complex, де зазначено).
Для дійсного аргумента результат дійсний; для Complex — комплексний.</p>
<article class="fn" id="sin">
<div class="fn-sig"><code>sin(x)</code></div>
<div class="fn-body"><p>Синус. Для дійсного x — Real; для Complex — Complex.</p></div>
<div class="ex-label">Приклад</div><pre>print(sin(0));
print(sin(pi / 2));  // ≈ 1</pre>
</article><article class="fn" id="cos">
<div class="fn-sig"><code>cos(x)</code></div>
<div class="fn-body"><p>Косинус.</p></div>
<div class="ex-label">Приклад</div><pre>print(cos(0));  // 1.0</pre>
</article><article class="fn" id="tan">
<div class="fn-sig"><code>tan(x)</code></div>
<div class="fn-body"><p>Тангенс. Біля π/2 + kπ дійсний результат може дати NonFinite.</p></div>
<div class="ex-label">Приклад</div><pre>print(tan(0));</pre>
</article><article class="fn" id="asin">
<div class="fn-sig"><code>asin(x)</code></div>
<div class="fn-body"><p>Арксинус. Для дійсних типовий діапазон аргумента [−1, 1].</p></div>
<div class="ex-label">Приклад</div><pre>print(asin(1));  // ≈ π/2</pre>
</article><article class="fn" id="acos">
<div class="fn-sig"><code>acos(x)</code></div>
<div class="fn-body"><p>Арккосинус.</p></div>
<div class="ex-label">Приклад</div><pre>print(acos(1));  // 0.0</pre>
</article><article class="fn" id="atan">
<div class="fn-sig"><code>atan(x)</code></div>
<div class="fn-body"><p>Арктангенс одного аргумента.</p></div>
<div class="ex-label">Приклад</div><pre>print(atan(0));</pre>
</article><article class="fn" id="atan2">
<div class="fn-sig"><code>atan2(y, x)</code></div>
<div class="fn-body"><p>Арктангенс частки y/x з урахуванням квадранта (як у математичних бібліотеках). Обидва аргументи — дійсні числа; Complex — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>print(atan2(0, 1));  // 0.0
print(atan2(1, 0));  // ≈ π/2</pre>
</article>

<h2 id="hyper">Гіперболічні</h2>
<article class="fn" id="sinh">
<div class="fn-sig"><code>sinh(x)</code></div>
<div class="fn-body"><p>Гіперболічний синус.</p></div>
<div class="ex-label">Приклад</div><pre>print(sinh(0));  // 0.0</pre>
</article><article class="fn" id="cosh">
<div class="fn-sig"><code>cosh(x)</code></div>
<div class="fn-body"><p>Гіперболічний косинус.</p></div>
<div class="ex-label">Приклад</div><pre>print(cosh(0));  // 1.0</pre>
</article><article class="fn" id="tanh">
<div class="fn-sig"><code>tanh(x)</code></div>
<div class="fn-body"><p>Гіперболічний тангенс.</p></div>
<div class="ex-label">Приклад</div><pre>print(tanh(0));  // 0.0</pre>
</article>

<h2 id="powlog">Корінь, exp, логарифми, степінь</h2>
<article class="fn" id="sqrt">
<div class="fn-sig"><code>sqrt(x)</code></div>
<div class="fn-body"><p>Квадратний корінь. Для відʼємного дійсного повертає Complex (уявний результат), а не помилку.</p></div>
<div class="ex-label">Приклад</div><pre>print(sqrt(9));     // 3.0
print(sqrt(-1));    // 1i</pre>
</article><article class="fn" id="exp">
<div class="fn-sig"><code>exp(x)</code></div>
<div class="fn-body"><p>e<sup>x</sup>.</p></div>
<div class="ex-label">Приклад</div><pre>print(exp(0));  // 1.0
print(exp(1));  // ≈ e</pre>
</article><article class="fn" id="ln">
<div class="fn-sig"><code>ln(x)  /  log(x)</code></div>
<div class="fn-body"><p>Натуральний логарифм. Імʼя <code>log</code> — повний синонім <code>ln</code>.</p></div>
<div class="ex-label">Приклад</div><pre>print(ln(e));   // 1.0
print(log(e));  // 1.0</pre>
</article><article class="fn" id="log10">
<div class="fn-sig"><code>log10(x)</code></div>
<div class="fn-body"><p>Десятковий логарифм.</p></div>
<div class="ex-label">Приклад</div><pre>print(log10(1000));  // 3.0</pre>
</article><article class="fn" id="log2">
<div class="fn-sig"><code>log2(x)</code></div>
<div class="fn-body"><p>Логарифм за основою 2.</p></div>
<div class="ex-label">Приклад</div><pre>print(log2(8));  // 3.0</pre>
</article><article class="fn" id="pow">
<div class="fn-sig"><code>pow(base, exp)</code></div>
<div class="fn-body"><p>Степінь base<sup>exp</sup>. Еквівалентно оператору <code>**</code> за змістом для підтримуваних типів.</p></div>
<div class="ex-label">Приклад</div><pre>print(pow(2, 10));  // 1024</pre>
</article><article class="fn" id="hypot">
<div class="fn-sig"><code>hypot(x, y)</code></div>
<div class="fn-body"><p>√(x² + y²) без зайжного переповнення проміжних квадратів. Лише дійсні аргументи.</p></div>
<div class="ex-label">Приклад</div><pre>print(hypot(3, 4));  // 5.0</pre>
</article>

<h2 id="abs">Модуль і знак</h2>
<article class="fn" id="abs">
<div class="fn-sig"><code>abs(x)</code></div>
<div class="fn-body"><p>Модуль: для Integer / Rational зберігає тип (з урахуванням переповнення для найменшого Integer); для Real — |x|; для Complex — модуль (Real).</p></div>
<div class="ex-label">Приклад</div><pre>print(abs(-3));
print(abs(3 + 4i));  // 5.0</pre>
</article><article class="fn" id="sign">
<div class="fn-sig"><code>sign(x)</code></div>
<div class="fn-body"><p>Знак дійсного числа: −1, 0 або 1 (Integer). Complex — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>print(sign(-5));  // -1
print(sign(0));   // 0
print(sign(2.5)); // 1</pre>
</article>

<h2 id="cplx">Комплексні</h2>
<article class="fn" id="re">
<div class="fn-sig"><code>re(z)</code></div>
<div class="fn-body"><p>Дійсна частина (Real). Для звичайного числа — воно саме як Real.</p></div>
<div class="ex-label">Приклад</div><pre>print(re(3 + 4i));  // 3.0</pre>
</article><article class="fn" id="im">
<div class="fn-sig"><code>im(z)</code></div>
<div class="fn-body"><p>Уявна частина. Для дійсного — 0.</p></div>
<div class="ex-label">Приклад</div><pre>print(im(3 + 4i));  // 4.0</pre>
</article><article class="fn" id="arg">
<div class="fn-sig"><code>arg(z)</code></div>
<div class="fn-body"><p>Аргумент (фаза) в радіанах.</p></div>
<div class="ex-label">Приклад</div><pre>print(arg(1 + 0i));  // 0.0</pre>
</article><article class="fn" id="conj">
<div class="fn-sig"><code>conj(z)</code></div>
<div class="fn-body"><p>Комплексно спряжене. Для дійсного / цілого / дробу повертає аргумент без змін.</p></div>
<div class="ex-label">Приклад</div><pre>print(conj(3 + 4i));  // 3 - 4i</pre>
</article>

<h2 id="round">Округлення</h2>
<p>Лише дійсні типи (Integer / Rational / Real). Результат завжди <b>Real</b>
(навіть від цілого). Complex — помилка. Щоб отримати Integer — обгорніть у
<a href="help:casts">int(...)</a>.</p>
<article class="fn" id="floor">
<div class="fn-sig"><code>floor(x)</code></div>
<div class="fn-body"><p>Найбільше ціле ≤ x (як дійсне).</p></div>
<div class="ex-label">Приклад</div><pre>print(floor(3.7));   // 3.0
print(floor(-3.2));  // -4.0</pre>
</article><article class="fn" id="ceil">
<div class="fn-sig"><code>ceil(x)</code></div>
<div class="fn-body"><p>Найменше ціле ≥ x.</p></div>
<div class="ex-label">Приклад</div><pre>print(ceil(3.2));  // 4.0</pre>
</article><article class="fn" id="round">
<div class="fn-sig"><code>round(x)</code></div>
<div class="fn-body"><p>Округлення до найближчого (як <code>std::round</code>:
половина від нуля вгору, напр. 2.5→3, −2.5→−3).</p></div>
<div class="ex-label">Приклад</div><pre>print(round(3.5));  // 4.0</pre>
</article><article class="fn" id="trunc">
<div class="fn-sig"><code>trunc(x)</code></div>
<div class="fn-body"><p>Обрізання дробової частини до нуля.</p></div>
<div class="ex-label">Приклад</div><pre>print(trunc(-3.9));  // -3.0</pre>
</article>

<pre>print(int(floor(3.7)));  // 3
print(int(trunc(-3.9))); // -3</pre>

<h2 id="minmax">min / max</h2>
<article class="fn" id="min">
<div class="fn-sig"><code>min(a, b)</code></div>
<div class="fn-body"><p>Менше з двох дійсних чисел. Якщо обидва Integer — результат Integer.</p></div>
<div class="ex-label">Приклад</div><pre>print(min(3, 1));  // 1</pre>
</article><article class="fn" id="max">
<div class="fn-sig"><code>max(a, b)</code></div>
<div class="fn-body"><p>Більше з двох.</p></div>
<div class="ex-label">Приклад</div><pre>print(max(3, 1));  // 3</pre>
</article>

<h2 id="angles">Кути: градуси ↔ радіани</h2>
<article class="fn" id="deg">
<div class="fn-sig"><code>deg(x)</code></div>
<div class="fn-body"><p>Радіани → градуси.</p></div>
<div class="ex-label">Приклад</div><pre>print(deg(pi));  // 180.0</pre>
</article><article class="fn" id="rad">
<div class="fn-sig"><code>rad(x)</code></div>
<div class="fn-body"><p>Градуси → радіани.</p></div>
<div class="ex-label">Приклад</div><pre>print(rad(180));  // ≈ pi
print(sin(rad(90)));  // ≈ 1</pre>
</article>

<h2 id="coll">Колекції</h2>
<article class="fn" id="len">
<div class="fn-sig"><code>len(a)</code></div>
<div class="fn-body"><p>Довжина масиву або кількість полів запису → Integer. Інші типи — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>print(len([1, 2, 3]));
print(len({x: 1, y: 2}));</pre>
</article><article class="fn" id="push">
<div class="fn-sig"><code>push(arr, x)</code></div>
<div class="fn-body"><p>Додає x у кінець масиву <b>на місці</b> (масиви за посиланням) і повертає той самий масив.</p></div>
<div class="ex-label">Приклад</div><pre>let a = [1, 2];
push(a, 3);
print(a);  // [1, 2, 3]</pre>
</article><article class="fn" id="pop">
<div class="fn-sig"><code>pop(arr)</code></div>
<div class="fn-body"><p>Знімає й повертає останній елемент. Порожній масив — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>let a = [1, 2, 3];
print(pop(a));  // 3
print(a);       // [1, 2]</pre>
</article><article class="fn" id="clone">
<div class="fn-sig"><code>clone(x)</code></div>
<div class="fn-body"><p>Глибока копія: для масиву/запису — новий обʼєкт у памʼяті (вкладені
масиви й записи теж копіюються; цикли зберігаються коректно). Інші типи
повертаються як є.</p></div>
<div class="ex-label">Приклад</div><pre>let a = [1, 2];
let b = clone(a);
b[0] = 9;
print(a[0]);     // 1
print(same(a, b)); // false</pre>
</article><article class="fn" id="same">
<div class="fn-sig"><code>same(a, b)</code></div>
<div class="fn-body"><p>Чи <code>a</code> і <code>b</code> — <b>той самий</b> масив або запис у памʼяті
(посилання). Для чисел/інших типів і змішаної пари Array/Record — завжди
<code>false</code>. Не плутати з <code>==</code> (порівняння за вмістом).</p></div>
<div class="ex-label">Приклад</div><pre>let a = [1, 2];
let b = a;
let c = [1, 2];
print(same(a, b));  // true
print(same(a, c));  // false
print(a == c);      // true</pre>
</article>

<h2 id="castfn">Приведення типів</h2>
<p>Повний опис правил — у розділі <a href="help:casts">Приведення типів</a>. Коротко:</p>
<article class="fn" id="int">
<div class="fn-sig"><code>int(x)</code></div>
<div class="fn-body"><p>→ Integer (trunc до 0). Нецілий Rational / Complex / Boolean — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>print(int(3.9));  // 3</pre>
</article><article class="fn" id="real">
<div class="fn-sig"><code>real(x)</code></div>
<div class="fn-body"><p>→ Real з Integer / Rational / Real.</p></div>
<div class="ex-label">Приклад</div><pre>print(real(1/2));  // 0.5</pre>
</article><article class="fn" id="bool">
<div class="fn-sig"><code>bool(x)</code></div>
<div class="fn-body"><p>→ Boolean. Нуль → false, інакше true; NaN — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>print(bool(0));  // false</pre>
</article><article class="fn" id="complex">
<div class="fn-sig"><code>complex(re)  /  complex(re, im)</code></div>
<div class="fn-body"><p>Один або два дійсні аргументи → Complex.</p></div>
<div class="ex-label">Приклад</div><pre>print(complex(2, 3));  // 2 + 3i</pre>
</article><article class="fn" id="rational">
<div class="fn-sig"><code>rational(n, d)</code></div>
<div class="fn-body"><p>Два Integer → скорочений Rational. d = 0 — помилка.</p></div>
<div class="ex-label">Приклад</div><pre>print(rational(2, 4));  // 1/2</pre>
</article>

<h2 id="istypefn">Перевірка типу</h2>
<p>Повертають Boolean. Зручно для змішаних масивів / полів. Повний опис —
у <a href="help:casts">Приведення типів</a> (розділ is_*).</p>
<article class="fn" id="is_int">
<div class="fn-sig"><code>is_int(x)</code> · <code>is_bool</code> · <code>is_rational</code> ·
<code>is_real</code> · <code>is_complex</code></div>
<div class="fn-body"><p>Точна перевірка відповідного типу (без приведення).</p></div>
<div class="ex-label">Приклад</div><pre>print(is_int(1));     // true
print(is_int(1.0));   // false
print(is_real(1.0));  // true</pre>
</article><article class="fn" id="is_number">
<div class="fn-sig"><code>is_number(x)</code></div>
<div class="fn-body"><p>Integer, Rational, Real або Complex.</p></div>
<div class="ex-label">Приклад</div><pre>print(is_number(1/2));  // true
print(is_number(true)); // false</pre>
</article><article class="fn" id="is_array">
<div class="fn-sig"><code>is_array(x)</code> · <code>is_record(x)</code> · <code>is_string(x)</code></div>
<div class="fn-body"><p>Колекції та ключ-рядок з for-in по запису.</p></div>
<div class="ex-label">Приклад</div><pre>print(is_array([1]));
print(is_record({}));
let p = {x: 1};
for (k in p) { print(is_string(k)); }</pre>
</article>

<div class="callout ok"><div class="callout-title">На замітку</div><p>У редакторі наведіть курсор на імʼя builtin — буде коротка підказка.
Деталі й приклади — у цьому розділі. Константи на кшталт <code>pi</code> —
<a href="help:constants">окремо</a>.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:casts">Приведення</a> · <a href="help:constants">Константи</a> · <a href="help:expr">Вирази</a></nav><nav class="pager"><a class="navprev" href="help:functions">← Функції</a> <a class="navnext" href="help:constants">Константи →</a></nav>
)HTML"},
    {"constants", "Константи", R"HTML(
<p class="lead">Імена без дужок. Де доречно — SI (CODATA / точні значення).</p>
<h1>Вбудовані константи</h1>

<p><code>let</code> з тим самим імʼям тимчасово ховає константу в поточній області.
Не можна оголосити <code>fun</code> з іменем константи.</p>

<nav class="toc">
<div class="toc-title">Зміст</div>
<ul>
<li><a href="#mathc">Математика</a></li>
<li><a href="#phys">Фізика (SI)</a></li>
<li><a href="#chem">Хімія / термодинаміка</a></li>
</ul>
</nav>

<h2 id="mathc">Математика</h2>
<table>
<tr><td><code>pi</code></td><td>π ≈ 3.14159…</td><td>число пі</td></tr>
<tr><td><code>e</code></td><td>≈ 2.71828…</td><td>основа натурального логарифма
(<b>не</b> заряд електрона)</td></tr>
<tr><td><code>tau</code></td><td>2π</td><td>повний оберт у радіанах</td></tr>
<tr><td><code>phi</code></td><td>(1+√5)/2 ≈ 1.618</td><td>золотий переріз</td></tr>
<tr><td><code>sqrt2</code></td><td>√2</td><td></td></tr>
<tr><td><code>sqrt3</code></td><td>√3</td><td></td></tr>
<tr><td><code>ln2</code></td><td>ln(2)</td><td></td></tr>
<tr><td><code>ln10</code></td><td>ln(10)</td><td></td></tr>
<tr><td><code>i</code></td><td>0 + 1i</td><td>уявна одиниця</td></tr>
</table>

<h2 id="phys">Фізика (SI)</h2>
<table>
<tr><td><code>c</code></td><td>299792458 м/с</td><td>швидкість світла у вакуумі (точно)</td></tr>
<tr><td><code>G</code></td><td>6.67430×10⁻¹¹ м³/(кг·с²)</td><td>гравітаційна стала</td></tr>
<tr><td><code>h</code></td><td>6.62607015×10⁻³⁴ Дж·с</td><td>стала Планка (точно)</td></tr>
<tr><td><code>hbar</code></td><td>h/(2π)</td><td>стала Дірака</td></tr>
<tr><td><code>qe</code></td><td>1.602176634×10⁻¹⁹ Кл</td><td>елементарний заряд (точно)</td></tr>
<tr><td><code>me</code></td><td>≈ 9.109×10⁻³¹ кг</td><td>маса електрона</td></tr>
<tr><td><code>mp</code></td><td>≈ 1.673×10⁻²⁷ кг</td><td>маса протона</td></tr>
<tr><td><code>mn</code></td><td>≈ 1.675×10⁻²⁷ кг</td><td>маса нейтрона</td></tr>
<tr><td><code>u</code></td><td>≈ 1.661×10⁻²⁷ кг</td><td>атомна одиниця маси</td></tr>
<tr><td><code>eps0</code></td><td>≈ 8.854×10⁻¹² Ф/м</td><td>електрична стала вакууму</td></tr>
<tr><td><code>mu0</code></td><td>≈ 1.257×10⁻⁶ Гн/м</td><td>магнітна стала вакууму</td></tr>
<tr><td><code>alpha</code></td><td>≈ 7.297×10⁻³</td><td>стала тонкої структури</td></tr>
<tr><td><code>sigma</code></td><td>≈ 5.670×10⁻⁸ Вт/(м²·К⁴)</td><td>стала Стефана–Больцмана</td></tr>
<tr><td><code>g</code></td><td>9.80665 м/с²</td><td>стандартне прискорення вільного падіння</td></tr>
<tr><td><code>atm</code></td><td>101325 Па</td><td>стандартна атмосфера (точно)</td></tr>
<tr><td><code>Ry</code></td><td>≈ 2.180×10⁻¹⁸ Дж</td><td>енергія Рідберга</td></tr>
<tr><td><code>a0</code></td><td>≈ 5.292×10⁻¹¹ м</td><td>боровський радіус</td></tr>
</table>

<h2 id="chem">Хімія / термодинаміка</h2>
<table>
<tr><td><code>NA</code></td><td>6.02214076×10²³ 1/моль</td><td>стала Авогадро (точно)</td></tr>
<tr><td><code>kB</code>, <code>k</code></td><td>1.380649×10⁻²³ Дж/К</td><td>стала Больцмана
(<code>k</code> — скорочення)</td></tr>
<tr><td><code>R</code></td><td>≈ 8.314 Дж/(моль·К)</td><td>універсальна газова стала</td></tr>
<tr><td><code>F</code></td><td>NA·qe</td><td>стала Фарадея (Кл/моль)</td></tr>
<tr><td><code>Vm</code></td><td>≈ 0.0224 м³/моль</td><td>молярний обʼєм ідеального газу (н.у.)</td></tr>
<tr><td><code>celsius0</code></td><td>273.15 K</td><td>0 °C у кельвінах</td></tr>
</table>

<pre>print("{:.4f}", pi);
print(c);
print(1 + i);
print(NA * qe == F);</pre>

<div class="callout ok"><div class="callout-title">На замітку</div><p>Заряд електрона — <code>qe</code>. Імʼя <code>e</code> зайняте числом Ейлера. Тригонометрія й <code>exp</code>/<code>ln</code> — у <a href="help:builtins">Builtins</a>.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:builtins">Builtins</a> · <a href="help:types">Типи</a></nav><nav class="pager"><a class="navprev" href="help:builtins">← Builtins</a> <a class="navnext" href="help:arrays">Масиви →</a></nav>
)HTML"},
    {"arrays", "Масиви", R"HTML(
<p class="lead">Списки значень; присвоєння ділить один і той самий масив.
Елементи можуть бути різного типу — окремого «масиву з T» немає.</p>
<h1>Масиви</h1>

<nav class="toc">
<div class="toc-title">Зміст розділу</div>
<ul>
<li><a href="#arr-basic">Створення і доступ</a></li>
<li><a href="#arr-mut">Зміна: присвоєння, push, pop</a></li>
<li><a href="#arr-ref">Посилання vs копія</a></li>
<li><a href="#arr-clone">clone і same</a></li>
<li><a href="#arr-eq">Порівняння</a></li>
<li><a href="#arr-iter">Обхід for-in</a></li>
<li><a href="#arr-nest">Вкладеність і змішані типи</a></li>
<li><a href="#arr-ok">Що можна</a></li>
<li><a href="#arr-no">Що не можна</a></li>
</ul>
</nav>

<h2 id="arr-basic">Створення і доступ</h2>
<pre>let a = [10, 20, 30];
let empty = [];
print(a);        // [10, 20, 30]
print(a[0]);     // 10 — індекси з нуля
print(a[2]);     // 30
print(len(a));   // 3
print(len([]));  // 0</pre>
<p>Індекс має бути <b>Integer</b> у півінтервалі <code>[0, len(a))</code>.
Відʼємні індекси й «з кінця» (<code>a[-1]</code>) <b>не підтримуються</b>.</p>

<h2 id="arr-mut">Зміна: присвоєння, push, pop</h2>
<pre>let a = [1, 2, 3];
a[1] = 9;          // заміна елемента
print(a);          // [1, 9, 3]

push(a, 4);        // додати в кінець (на місці)
print(a);          // [1, 9, 3, 4]

print(pop(a));     // 4 — знятий елемент
print(a);          // [1, 9, 3]
print(len(a));     // 3</pre>
<p><code>push</code> повертає той самий масив (зручно вкладати виклики).
<code>pop</code> з порожнього масиву — помилка.</p>
<pre>let xs = [];
push(xs, 1);
push(xs, push([10], 20));  // другий елемент — масив [10, 20]
print(xs);                 // [1, [10, 20]]</pre>

<div class="callout tip"><div class="callout-title">Порада</div><p>Повний опис <code>len</code> / <code>push</code> / <code>pop</code> /
<code>clone</code> / <code>same</code> — у <a href="help:builtins">Builtins</a>
(розділ «Колекції»).</p></div>

<h2 id="arr-ref">Посилання vs копія</h2>
<p>Присвоєння <code>let b = a</code> <b>не копіює</b> елементи: обидві змінні
вказують на одні й ті самі дані.</p>
<pre>let a = [1, 2, 3];
let b = a;
b[0] = 9;
print(a[0]);  // 9 — змінилось і в a
print(b[0]);  // 9
print(same(a, b));  // true — один обʼєкт</pre>

<h2 id="arr-clone">clone і same</h2>
<p><code>clone(a)</code> робить <b>глибоку</b> копію: новий масив, і всі вкладені
масиви/записи теж нові. Цикли (масив, що містить себе) копіюються коректно.</p>
<pre>let a = [1, 2, 3];
let c = clone(a);
c[0] = 100;
print(a[0]);       // 1 — оригінал не змінено
print(c[0]);       // 100
print(same(a, c)); // false
print(a == c);     // false (вміст уже різний)</pre>
<pre>let nested = [{x: 1}, {x: 2}];
let copy = clone(nested);
copy[0].x = 9;
print(nested[0].x);  // 1
print(copy[0].x);    // 9</pre>
<p><code>same(a, b)</code> — чи це <b>той самий</b> обʼєкт у памʼяті.
<code>==</code> порівнює лише вміст.</p>
<pre>let a = [1, 2];
let b = a;
let c = [1, 2];
print(same(a, b));  // true
print(same(a, c));  // false
print(a == c);      // true</pre>

<h2 id="arr-eq">Порівняння</h2>
<p><code>==</code> / <code>!=</code> порівнюють масиви <b>за вмістом</b>
(глибоко). Тотожність посилання — окремо через <code>same</code>:</p>
<pre>print([1, 2] == [1, 2]);  // true
print([1, 2] == [1, 3]);  // false

let a = [1, 2];
let b = a;
print(a == b);            // true
print(same(a, b));        // true</pre>
<p>Циклічні структури друкуються скорочено (<code>[...]</code>), порівняння
з собою не зациклюється:</p>
<pre>let a = [1];
a[0] = a;
print(a);      // [...]
print(a == a); // true
print(same(a, a[0])); // true</pre>

<h2 id="arr-iter">Обхід for-in</h2>
<pre>let s = 0;
for (x in [1, 2, 3]) {
  s = s + x;
}
print(s);  // 6

for (i, v in [10, 20]) {
  print("{}:{}", i, v);  // 0:10 потім 1:20
}</pre>
<div class="callout note"><div class="callout-title">Зауваження</div><p>Довжина для ітерації фіксується на старті: <code>push</code> у тілі
подовжує масив, але <b>не</b> додає нових кроків цього циклу.</p></div>
<pre>let a = [1];
let n = 0;
for (x in a) {
  push(a, 2);
  n = n + 1;
}
print(n);       // 1 — лише один прохід
print(len(a));  // 2 — елемент таки додано</pre>

<h2 id="arr-nest">Вкладеність і змішані типи</h2>
<p>Елементи можуть бути різних типів, зокрема інші масиви й
<a href="help:records">записи</a>. Тип елемента зʼясовуйте через
<a href="help:casts">is_int / is_array / …</a>:</p>
<pre>let mixed = [1, true, 1/2, {x: 1}];
print(mixed);  // [1, true, 1/2, {x: 1}]
for (v in mixed) {
  if (is_number(v)) { print(v); }
}

let grid = [[1, 2], [3, 4]];
print(grid[1][0]);  // 3

let r = { pts: [{x: 1}, {x: 2}] };
print(r.pts[1].x);  // 2
r.pts[0].x = 9;
print(r.pts[0].x);  // 9</pre>

<h2 id="arr-ok">Що можна</h2>
<ul>
<li>Порожній літерал <code>[]</code>, літерал з будь-якими значеннями.</li>
<li>Читання / запис <code>a[i]</code> з цілим індексом у межах.</li>
<li><code>len</code>, <code>push</code>, <code>pop</code>.</li>
<li><code>clone</code> — глибока незалежна копія; <code>same</code> — тотожність
посилання.</li>
<li>Передавати масив у <code>fun</code> і змінювати його всередині
(зміни видно ззовні — посилання).</li>
<li>Порівнювати <code>==</code> / <code>!=</code> за вмістом.</li>
<li>Обходити через <a href="help:forin">for-in</a> (елементи або індекс+елемент).</li>
<li>Вкладати масиви в масиви й у поля записів.</li>
</ul>

<h2 id="arr-no">Що не можна</h2>
<table>
<tr><td><code>a[-1]</code>, <code>a[len(a)]</code></td>
<td>вихід за межі — помилка</td></tr>
<tr><td><code>a[1.0]</code>, <code>a[1/2]</code></td>
<td>індекс має бути Integer, не Real/Rational</td></tr>
<tr><td><code>a + b</code>, <code>a * 2</code></td>
<td>немає арифметики / конкатенації масивів</td></tr>
<tr><td><code>pop([])</code></td>
<td>порожній масив — помилка</td></tr>
<tr><td><code>for (x in 1)</code></td>
<td>ітерувати можна лише масив або запис</td></tr>
<tr><td>зріз на кшталт <code>a[1:3]</code></td>
<td>синтаксису зрізів немає</td></tr>
<tr><td>автокопія при <code>let b = a</code></td>
<td>це посилання; для копії — <code>clone(a)</code></td></tr>
</table>

<pre>// ці рядки завершаться помилкою, якщо розкоментувати:
// print([1][-1]);
// print([1][1]);
// print([1][0.0]);
// print(pop([]));
// print([1, 2] + [3]);</pre>

<div class="callout warn"><div class="callout-title">Увага</div><p>Помилка індексації / <code>pop</code> підкреслюється в редакторі й
пишеться в <a href="help:panels">Консоль</a> як повідомлення виконання.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:records">Записи</a> · <a href="help:forin">for-in</a> · <a href="help:builtins">clone / same</a></nav><nav class="pager"><a class="navprev" href="help:constants">← Константи</a> <a class="navnext" href="help:records">Записи →</a></nav>
)HTML"},
    {"records", "Записи", R"HTML(
<p class="lead">Записи з полями. Як масиви: за посиланням; <code>==</code>
порівнює вміст.</p>
<h1>Записи (обʼєкти)</h1>

<nav class="toc">
<div class="toc-title">Зміст розділу</div>
<ul>
<li><a href="#rec-basic">Літерал і доступ до полів</a></li>
<li><a href="#rec-block">Запис чи блок коду?</a></li>
<li><a href="#rec-mut">Нові поля і присвоєння</a></li>
<li><a href="#rec-ref">Посилання</a></li>
<li><a href="#rec-has">Оператор has</a></li>
<li><a href="#rec-clone">clone і same</a></li>
<li><a href="#rec-key">Доступ через ключ p[k]</a></li>
<li><a href="#rec-iter">Обхід for-in</a></li>
<li><a href="#rec-nest">Вкладеність з масивами</a></li>
<li><a href="#rec-ok">Що можна</a></li>
<li><a href="#rec-no">Що не можна</a></li>
</ul>
</nav>

<h2 id="rec-basic">Літерал і доступ до полів</h2>
<pre>let p = {x: 1, y: 2};
print(p);      // {x: 1, y: 2}
print(p.x);    // 1
print(p.y);    // 2
print(len(p)); // 2 — кількість полів

let empty = {};
print(empty);  // {}
print(len({})); // 0</pre>
<p>Імена полів у літералі — ідентифікатори (<code>x</code>, <code>pts</code>, …),
не рядкові літерали в лапках.</p>

<h2 id="rec-block">Запис чи блок коду?</h2>
<p>Одні й ті самі дужки <code>{ }</code> у мові означають різне залежно від
контексту:</p>
<ul>
<li>у <b>виразі</b> (права частина <code>let</code>, аргумент, елемент масиву) —
запис з полями <code>імʼя: значення</code>;</li>
<li>як <b>тіло</b> <code>if</code> / <code>while</code> / <code>fun</code> /
окремий блок операторів — звичайний блок коду.</li>
</ul>
<pre>// запис у виразі
let a = { v: 1 };
print(a.v);  // 1

// блок операторів (не запис)
{
  let b = { v: 2 };
  print(b.v);  // 2
}</pre>

<h2 id="rec-mut">Нові поля і присвоєння</h2>
<pre>let p = {x: 1};
p.x = 5;     // змінити існуюче
p.z = 7;     // додати нове поле
print(p);    // {x: 5, z: 7}
print(len(p)); // 2</pre>
<div class="callout warn"><div class="callout-title">Увага</div><p>Читання <b>відсутнього</b> поля — помилка: <code>print(p.y)</code>,
якщо <code>y</code> ще не створювали. Спочатку присвойте <code>p.y = …</code>
або перевірте <code>p has y</code>.</p></div>

<h2 id="rec-ref">Посилання</h2>
<pre>let p = {x: 1};
let q = p;
q.x = 5;
print(p.x);  // 5 — той самий запис
p.z = 7;
print(q.z);  // 7
print(len(p)); // 2
print(same(p, q)); // true</pre>
<p>Порівняння <code>==</code> — за вмістом полів; тотожність у памʼяті —
<code>same</code>:</p>
<pre>print({a: 1} == {a: 1});  // true
print({a: 1} == {a: 2});  // false
print(same({a: 1}, {a: 1})); // false — два різні обʼєкти</pre>
<p>Цикл на себе друкується як <code>{x: ...}</code>:</p>
<pre>let p = {x: 1};
p.x = p;
print(p);      // {x: ...}
print(p == p); // true
print(same(p, p.x)); // true</pre>

<h2 id="rec-has">Оператор has</h2>
<p><code>record has field</code> — чи є поле з таким імʼям. Праворуч лише
ідентифікатор (як у <code>p.x</code>), не вираз. Результат — Boolean.
Читання відсутнього поля — помилка; <code>has</code> дозволяє перевірити
заздалегідь.</p>
<pre>let p = {x: 1};
print(p has x);  // true
print(p has y);  // false

if (not (p has y)) {
  p.y = 0;
}
print(p.y);  // 0</pre>
<p>На не-записі (масив, число, …) — помилка.</p>

<h2 id="rec-clone">clone і same</h2>
<p><code>clone(p)</code> — глибока копія запису (включно з вкладеними
масивами й записами). Після <code>clone</code> зміни в копії не чіпають
оригінал.</p>
<pre>let r = { pts: [{x: 1}, {x: 2}] };
let c = clone(r);
c.pts[0].x = 9;
print(r.pts[0].x);     // 1
print(c.pts[0].x);     // 9
print(same(r, c));     // false
print(same(r.pts, c.pts)); // false</pre>

<h2 id="rec-key">Доступ через ключ p[k]</h2>
<p>Літерал <code>p["x"]</code> написати не можна (рядковий літерал у виразах
заборонений). Ключ береться з <a href="help:forin">for-in</a> по запису —
тоді працює індексація:</p>
<pre>let p = {x: 1, y: 2};
for (k in p) {
  print(p[k]);  // 1 потім 2 (порядок полів як у літералі)
}
for (k, v in p) {
  print("{}={}", k, v);
}</pre>
<div class="callout note"><div class="callout-title">Зауваження</div><p><code>p[0]</code> з цілим індексом — помилка: запис індексується
ключем поля, не позицією. Для впорядкованих даних використовуйте
<a href="help:arrays">масив</a>.</p></div>

<h2 id="rec-iter">Обхід for-in</h2>
<pre>let o = {a: 1, b: 2};
let ks = [];
for (k in o) {
  push(ks, k);   // накопичити ключі в масиві
}
print(len(ks));  // 2

let sum = 0;
for (k, v in o) {
  sum = sum + v;
}
print(sum);  // 3</pre>
<p>Порожній запис: тіло циклу не виконується. <code>break</code> /
<code>continue</code> працюють як у звичайних циклах.</p>

<h2 id="rec-nest">Вкладеність з масивами</h2>
<pre>let scene = {
  name_len: 3,
  pts: [{x: 1, y: 0}, {x: 2, y: 5}]
};
print(scene.pts[1].y);  // 5
scene.pts[0].x = 9;
print(scene.pts[0].x);  // 9

// масив записів
let rows = [{id: 1}, {id: 2}];
for (i, row in rows) {
  print("{} -&gt; {}", i, row.id);
}</pre>

<h2 id="rec-ok">Що можна</h2>
<ul>
<li>Літерал <code>{a: 1, b: true}</code>, порожній <code>{}</code>.</li>
<li>Читання / запис <code>p.field</code>; створення нового поля присвоєнням.</li>
<li><code>len(p)</code> — кількість полів.</li>
<li><code>p has field</code> — перевірка наявності поля.</li>
<li>Посилальна семантика (<code>let q = p</code>) і глибоке <code>==</code>.</li>
<li><code>clone(p)</code> — незалежна глибока копія; <code>same(a, b)</code> —
чи той самий обʼєкт у памʼяті.</li>
<li>Обхід ключів або пар ключ–значення через <a href="help:forin">for-in</a>.</li>
<li>Доступ <code>p[k]</code>, коли <code>k</code> отримано з ітерації запису.</li>
<li>Вкладати записи в масиви й навпаки; передавати в функції.</li>
</ul>

<h2 id="rec-no">Що не можна</h2>
<table>
<tr><td><code>print(p.missing)</code></td>
<td>читання неіснуючого поля — помилка</td></tr>
<tr><td><code>p[0]</code>, <code>p[1.0]</code></td>
<td>числовий індекс для запису не працює</td></tr>
<tr><td><code>p["x"]</code></td>
<td>немає рядкових літералів як значень / ключів у коді</td></tr>
<tr><td><code>p + q</code>, злиття записів оператором</td>
<td>арифметики / «spread» немає</td></tr>
<tr><td>методи на кшталт <code>p.keys()</code></td>
<td>лише поля-значення; обхід — <code>for-in</code></td></tr>
<tr><td>оголосити поле без значення в літералі</td>
<td>завжди <code>імʼя: вираз</code></td></tr>
</table>

<pre>let p = {x: 1};
// помилки, якщо розкоментувати:
// print(p.y);
// print(p[0]);
// print(p + p);</pre>

<div class="callout tip"><div class="callout-title">Порада</div><p>Перед читанням необовʼязкового поля використовуйте
<code>if (p has field) { … p.field … }</code>. Для обходу всіх ключів —
<a href="help:forin">for-in</a>.</p></div>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:arrays">Масиви</a> · <a href="help:forin">for-in</a> · <a href="help:builtins">clone / same</a></nav><nav class="pager"><a class="navprev" href="help:arrays">← Масиви</a> <a class="navnext" href="help:arith">Арифметика →</a></nav>
)HTML"},
    {"arith", "Арифметика", R"HTML(
<p class="lead">Що виходить зі змішаних типів у арифметиці і які помилки
найчастіші.</p>
<h1>Арифметика і типові помилки</h1>

<ul>
<li>Integer лишається цілим, доки не зʼявиться Rational / Real / Complex у виразі.</li>
<li><code>1/2 + 1/2</code> → <code>1</code> (дроби); <code>1 / 2</code> →
<code>0.5</code> (ділення дійсних).</li>
<li>Real сам по собі не «звужується» до Integer — потрібен
<a href="help:casts">int(...)</a>.</li>
<li>Порівняння <code>&lt;</code> … не визначені для Complex.</li>
<li>Boolean не бере участі в арифметиці: <code>true + 1</code> — помилка.</li>
<li><code>==</code> / <code>!=</code> для масивів і записів — за вмістом
(глибоко); тотожність у памʼяті — <code>same</code>.</li>
</ul>

<p>Помилки виконання, які часто бачать новачки: переповнення цілого, ділення
на нуль, Inf/NaN у недоречному контексті, операція з непідхожим типом
(<code>1 and true</code>, <code>floor(1+2i)</code>).</p>

<pre>print(1/2 + 1/2);   // 1
print(1 / 2);        // 0.5
print(int(3.9));     // 3
print((-2) ** 3);    // -8</pre>

<p>Повний список функцій — <a href="help:builtins">Builtins</a>; логіка умов —
<a href="help:if">if / else</a>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:types">Типи</a> · <a href="help:casts">Приведення</a> · <a href="help:builtins">Builtins</a></nav><nav class="pager"><a class="navprev" href="help:records">← Записи</a> <a class="navnext" href="help:license">Ліцензія →</a></nav>
)HTML"},
    {"license", "Ліцензія", R"HTML(
<p class="lead">Вільна ліцензія (GPL).</p>
<h1>Ліцензія</h1>
<p><b>calx</b> і <b>calx-ide</b> поширюються під GNU GPL версії 3 або
(на ваш вибір) будь-якої пізнішої версії.</p>
<p>© 2026 Ihor Dolinskyi.</p>
<p>Сторонні бібліотеки (зокрема Qt для IDE, replxx для CLI) описані у файлі
NOTICE поруч із програмою або в дереві встановлення
<code>share/doc/calx</code>.</p>
<nav class="related"><span class="related-label">Також дивіться</span><a href="help:overview">Огляд</a></nav><nav class="pager"><a class="navprev" href="help:arith">← Арифметика</a> <span></span></nav>
)HTML"},

};

[[nodiscard]] int indexById(const QString &id) {
  for (int i = 0; i < static_cast<int>(std::size(kSections)); ++i) {
    if (id == QString::fromUtf8(kSections[static_cast<std::size_t>(i)].id)) {
      return i;
    }
  }
  return -1;
}

[[nodiscard]] int indexByTitle(const QString &title) {
  for (int i = 0; i < static_cast<int>(std::size(kSections)); ++i) {
    if (title ==
        QString::fromUtf8(kSections[static_cast<std::size_t>(i)].title)) {
      return i;
    }
  }
  return -1;
}

} // namespace


namespace {

[[nodiscard]] QString htmlUnescape(QString s) {
  s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return s;
}

[[nodiscard]] bool isIdentStart(QChar c) {
  return c.isLetter() || c == QLatin1Char('_');
}

[[nodiscard]] bool isIdentCont(QChar c) {
  return c.isLetterOrNumber() || c == QLatin1Char('_');
}

struct HighlightSpan {
  QString text;
  QColor color;
};

[[nodiscard]] QList<HighlightSpan> highlightCalxSpans(const QString &src) {
  static const QStringList kKeywords = {
      QStringLiteral("let"),      QStringLiteral("print"),
      QStringLiteral("if"),       QStringLiteral("else"),
      QStringLiteral("while"),    QStringLiteral("for"),
      QStringLiteral("in"),       QStringLiteral("break"),
      QStringLiteral("continue"), QStringLiteral("fun"),
      QStringLiteral("return"),   QStringLiteral("true"),
      QStringLiteral("false"),    QStringLiteral("and"),
      QStringLiteral("or"),       QStringLiteral("not"),
      QStringLiteral("has")};

  const QColor colKeyword(0x56d4ff);
  const QColor colBuiltin(0x7ee787);
  const QColor colConstant(0xdda6ff);
  const QColor colIdent(0xe6edf3);
  const QColor colNumber(0xffb86b);
  const QColor colString(0x9fdbff);
  const QColor colComment(0x8b9cb3);
  const QColor colOperator(0xff7b72);
  const QColor colPunct(0xc9d1d9);

  QList<HighlightSpan> out;
  const int n = static_cast<int>(src.size());
  int i = 0;
  while (i < n) {
    const QChar c = src[i];

    if (c == QLatin1Char('/') && i + 1 < n && src[i + 1] == QLatin1Char('/')) {
      int j = i;
      while (j < n && src[j] != QLatin1Char('\n')) {
        ++j;
      }
      out.append({src.mid(i, j - i), colComment});
      i = j;
      continue;
    }

    if (c == QLatin1Char('"')) {
      int j = i + 1;
      bool esc = false;
      while (j < n) {
        if (esc) {
          esc = false;
        } else if (src[j] == QLatin1Char('\\')) {
          esc = true;
        } else if (src[j] == QLatin1Char('"')) {
          ++j;
          break;
        }
        ++j;
      }
      out.append({src.mid(i, j - i), colString});
      i = j;
      continue;
    }

    if (c.isDigit() ||
        (c == QLatin1Char('.') && i + 1 < n && src[i + 1].isDigit())) {
      int j = i;
      if (c == QLatin1Char('.')) {
        ++j;
      }
      while (j < n && src[j].isDigit()) {
        ++j;
      }
      if (j < n && (src[j] == QLatin1Char('e') || src[j] == QLatin1Char('E'))) {
        int k = j + 1;
        if (k < n &&
            (src[k] == QLatin1Char('+') || src[k] == QLatin1Char('-'))) {
          ++k;
        }
        if (k < n && src[k].isDigit()) {
          j = k;
          while (j < n && src[j].isDigit()) {
            ++j;
          }
        }
      }
      if (j < n && src[j] == QLatin1Char('i') &&
          (j + 1 >= n || !isIdentCont(src[j + 1]))) {
        ++j;
      }
      if (j < n && src[j] == QLatin1Char('/') && j + 1 < n &&
          src[j + 1].isDigit()) {
        ++j;
        while (j < n && src[j].isDigit()) {
          ++j;
        }
      }
      out.append({src.mid(i, j - i), colNumber});
      i = j;
      continue;
    }

    if (isIdentStart(c)) {
      int j = i + 1;
      while (j < n && isIdentCont(src[j])) {
        ++j;
      }
      const QString id = src.mid(i, j - i);
      const QByteArray utf8 = id.toUtf8();
      const std::string_view sv(utf8.constData(),
                                static_cast<std::size_t>(utf8.size()));
      QColor color = colIdent;
      if (kKeywords.contains(id)) {
        color = colKeyword;
      } else if (calc::find_builtin(sv)) {
        color = colBuiltin;
      } else if (calc::is_constant_name(sv)) {
        color = colConstant;
      }
      out.append({id, color});
      i = j;
      continue;
    }

    if (i + 1 < n) {
      const QString two = src.mid(i, 2);
      if (two == QStringLiteral("**") || two == QStringLiteral("==") ||
          two == QStringLiteral("!=") || two == QStringLiteral("<=") ||
          two == QStringLiteral(">=")) {
        out.append({two, colOperator});
        i += 2;
        continue;
      }
    }

    if (QStringLiteral("+-*/%<>=!").contains(c)) {
      out.append({QString(c), colOperator});
      ++i;
      continue;
    }

    if (QStringLiteral("(){}[],;:.").contains(c)) {
      out.append({QString(c), colPunct});
      ++i;
      continue;
    }

    // пробіл / інше
    out.append({QString(c), colIdent});
    ++i;
  }
  return out;
}

} // namespace

QString HelpDialog::pageChrome(const QString &body_html) {
  return QStringLiteral(
             "<!DOCTYPE html><html><head><meta charset='utf-8'><style>"
             "body{font-family:'Source Sans 3','Noto Sans','Segoe UI',sans-serif;"
             "font-size:15.5px;line-height:1.62;color:#1c2128;background:#f7f8fa;"
             "margin:0;padding:1.35em 1.75em 2.4em;}"
             "p.lead{font-size:1.05em;color:#4a5563;margin:0 0 1.1em;"
             "line-height:1.55;max-width:42em;}"
             "h1{font-family:'Source Serif 4','Noto Serif','Georgia',serif;"
             "font-size:1.72em;color:#0f1419;margin:0 0 0.55em;font-weight:650;"
             "letter-spacing:-0.01em;line-height:1.25;}"
             "h2{font-size:1.14em;color:#1a2330;margin:1.55em 0 0.5em;"
             "font-weight:650;padding-bottom:0.28em;"
             "border-bottom:1px solid #e2e6eb;}"
             "h3{font-size:1.02em;margin:1.1em 0 0.35em;color:#243044;}"
             "p,li,td{color:#1c2128;}"
             "ul,ol{padding-left:1.35em;margin:0.45em 0 0.85em;}"
             "li{margin:0.42em 0;}"
             "table.grid,table.keys{border-collapse:collapse;margin:0.75em 0 1em;"
             "width:100%;font-size:0.97em;}"
             "table.grid td,table.keys td{padding:0.48em 0.85em 0.48em 0;"
             "vertical-align:top;}"
             "table.grid tr,table.keys tr{border-bottom:1px solid #e6e9ee;}"
             "table.keys td:first-child{white-space:nowrap;width:9.5em;}"
             "kbd{font-family:'JetBrains Mono','Noto Sans Mono',monospace;"
             "font-size:0.86em;border:1px solid #d0d5dd;"
             "border-bottom-width:2px;border-radius:4px;padding:0.1em 0.45em;"
             "color:#1a2330;}"
             /* inline code: без background — у Qt фон розтягує рядок на всю ширину */
             ".ic{font-family:'JetBrains Mono','Noto Sans Mono',monospace;"
             "font-size:0.92em;color:#0550ae;font-weight:600;}"
             "a{color:#1a6fa8;text-decoration:none;border-bottom:1px solid "
             "rgba(26,111,168,0.35);}"
             "a:hover{color:#0d5690;border-bottom-color:#0d5690;}"
             ".callout{padding:0.85em 1.05em;margin:1em 0;border-radius:8px;"
             "border:1px solid transparent;}"
             ".callout p{margin:0.25em 0 0;}"
             ".callout-title{font-size:0.72em;font-weight:700;letter-spacing:"
             "0.06em;text-transform:uppercase;margin:0;}"
             ".tip{background:#f4efe0;border-color:#e0d3a8;}"
             ".tip .callout-title{color:#7a6230;}"
             ".warn{background:#f8eaea;border-color:#e0b4b4;}"
             ".warn .callout-title{color:#8a3a3a;}"
             ".ok{background:#e7f2ea;border-color:#b5d4bf;}"
             ".ok .callout-title{color:#2f6b45;}"
             ".note{background:#e8eef6;border-color:#b8c9de;}"
             ".note .callout-title{color:#3a5678;}"
             ".fn{margin:0.95em 0 1.15em;padding:0.75em 0 0.75em 1em;"
             "border-left:3px solid #3d7ea6;background:#f0f3f7;"
             "border-radius:0 8px 8px 0;}"
             ".fn-sig{margin:0 0 0.35em;font-family:'JetBrains Mono',"
             "'Noto Sans Mono',monospace;font-size:0.95em;font-weight:650;"
             "color:#0d5690;}"
             ".fn-body{margin:0.2em 0 0.4em;}"
             ".fn-body p{margin:0.3em 0;}"
             ".ex-label{font-size:0.7em;font-weight:700;letter-spacing:0.05em;"
             "text-transform:uppercase;color:#5a6a7c;margin:0.55em 0 0.2em;}"
             ".toc{background:#eef2f6;border:1px solid #d9e0e8;border-radius:8px;"
             "padding:0.85em 1.1em;margin:0.9em 0 1.2em;}"
             ".toc-title{font-size:0.72em;font-weight:700;letter-spacing:0.06em;"
             "text-transform:uppercase;color:#5a6a7c;margin-bottom:0.35em;}"
             ".toc ul{margin:0.2em 0;padding-left:1.2em;}"
             ".toc li{margin:0.28em 0;}"
             ".related{margin:1.75em 0 0;padding:0.9em 1.05em;background:#eef2f6;"
             "border-radius:8px;border:1px solid #d9e0e8;line-height:1.7;}"
             ".related-label{font-size:0.72em;font-weight:700;"
             "letter-spacing:0.06em;text-transform:uppercase;color:#5a6a7c;"
             "margin:0 0 0.45em 0;}"
             ".related-links{margin:0;color:#1c2128;}"
             ".related-links a{margin:0 0.1em;}"
             "table.pager{width:100%;border-collapse:collapse;margin:1.35em 0 0.4em;"
             "border-top:1px solid #d0d5dd;}"
             "table.pager td{border:none;padding:1em 0 0.25em;vertical-align:middle;"
             "font-size:0.95em;}"
             "table.pager td.pager-prev{text-align:left;width:50%;}"
             "table.pager td.pager-next{text-align:right;width:50%;}"
             "a.navprev,a.navnext{color:#0d5690;border-bottom:none;font-weight:650;"
             "padding:0.35em 0.55em;background:#e8eef5;border-radius:6px;}"
             /* картки коду — растрові img (QTextBrowser не вміє нормально CSS width/radius) */
             "p.codeframe{text-align:center;margin:1.15em 0 1.35em;}"
             "p.codeframe img{border:none;}"
             "p.codeframe a.copybtn{color:#1a6fa8;border-bottom:none;font-size:12px;"
             "font-weight:600;margin-left:0.75em;}"
             "p.codeframe .codeframe-meta{color:#6a737d;font-size:11px;"
             "font-family:'JetBrains Mono','Noto Sans Mono',monospace;"
             "letter-spacing:0.04em;margin-bottom:0.35em;}"
             "</style></head><body>%1</body></html>")
      .arg(body_html);
}

QString HelpDialog::enrichHtml(const QString &body_html) {
  copySnippets_.clear();
  codeImages_.clear();
  QString html = body_html;

  // 1) inline <code> → <span class="ic"> (уникаємо бага Qt з фоном)
  {
    QString out;
    out.reserve(html.size());
    qsizetype pos = 0;
    while (true) {
      const qsizetype open = html.indexOf(QStringLiteral("<code>"), pos);
      if (open < 0) {
        out += html.mid(pos);
        break;
      }
      out += html.mid(pos, open - pos);
      const qsizetype content = open + 6;
      const qsizetype close = html.indexOf(QStringLiteral("</code>"), content);
      if (close < 0) {
        out += html.mid(open);
        break;
      }
      out += QStringLiteral("<span class=\"ic\">");
      out += html.mid(content, close - content);
      out += QStringLiteral("</span>");
      pos = close + 7;
    }
    html = out;
  }

  // 2) <pre> → растрова картка (фіксована ширина, радіус, padding) + Копіювати
  {
    const qreal dpr = browser_ ? browser_->devicePixelRatioF() : 1.0;
    QString out;
    out.reserve(html.size() + 256);
    qsizetype pos = 0;
    while (true) {
      const qsizetype open = html.indexOf(QStringLiteral("<pre>"), pos);
      if (open < 0) {
        out += html.mid(pos);
        break;
      }
      out += html.mid(pos, open - pos);
      const qsizetype content = open + 5;
      const qsizetype close = html.indexOf(QStringLiteral("</pre>"), content);
      if (close < 0) {
        out += html.mid(open);
        break;
      }
      QString raw = html.mid(content, close - content);
      if (raw.startsWith(QLatin1Char('\n'))) {
        raw = raw.mid(1);
      }
      if (raw.endsWith(QLatin1Char('\n'))) {
        raw.chop(1);
      }
      const QString plain = htmlUnescape(raw);
      const int idx = static_cast<int>(copySnippets_.size());
      copySnippets_.append(plain);
      const QImage card = renderCodeCard(plain, dpr);
      codeImages_.push_back(card);

      const qreal pr = qMax<qreal>(1.0, card.devicePixelRatio());
      const int logical_w = qRound(static_cast<qreal>(card.width()) / pr);
      const int logical_h = qRound(static_cast<qreal>(card.height()) / pr);

      // Клік по всій картці → копіювання (кнопка намальована в шапці зображення).
      out += QStringLiteral("<p class=\"codeframe\" align=\"center\">"
                            "<a href=\"copy:");
      out += QString::number(idx);
      out += QStringLiteral("\"><img src=\"codeblock:");
      out += QString::number(idx);
      out += QStringLiteral("\" width=\"");
      out += QString::number(logical_w);
      out += QStringLiteral("\" height=\"");
      out += QString::number(logical_h);
      out += QStringLiteral("\"/></a></p>");
      pos = close + 6;
    }
    html = out;
  }

  // 3) «Також дивіться» — окремий блок (label не в один рядок із посиланнями)
  {
    QString out;
    out.reserve(html.size() + 128);
    qsizetype pos = 0;
    const QString open_tag = QStringLiteral("<nav class=\"related\">");
    const QString label =
        QStringLiteral("<span class=\"related-label\">Також дивіться</span>");
    while (true) {
      const qsizetype open = html.indexOf(open_tag, pos);
      if (open < 0) {
        out += html.mid(pos);
        break;
      }
      out += html.mid(pos, open - pos);
      const qsizetype after_open = open + open_tag.size();
      const qsizetype close = html.indexOf(QStringLiteral("</nav>"), after_open);
      if (close < 0) {
        out += html.mid(open);
        break;
      }
      QString inner = html.mid(after_open, close - after_open);
      if (inner.startsWith(label)) {
        inner = inner.mid(label.size());
      }
      out += QStringLiteral(
          "<div class=\"related\">"
          "<p class=\"related-label\">Також дивіться</p>"
          "<p class=\"related-links\">");
      out += inner.trimmed();
      out += QStringLiteral("</p></div>");
      pos = close + 6;
    }
    html = out;
  }

  // 4) ← / → — окрема таблиця під «Також дивіться», не в один рядок із ним
  {
    QString out;
    out.reserve(html.size() + 128);
    qsizetype pos = 0;
    const QString open_tag = QStringLiteral("<nav class=\"pager\">");
    while (true) {
      const qsizetype open = html.indexOf(open_tag, pos);
      if (open < 0) {
        out += html.mid(pos);
        break;
      }
      out += html.mid(pos, open - pos);
      const qsizetype after_open = open + open_tag.size();
      const qsizetype close = html.indexOf(QStringLiteral("</nav>"), after_open);
      if (close < 0) {
        out += html.mid(open);
        break;
      }
      const QString inner = html.mid(after_open, close - after_open);

      QString prev_html;
      QString next_html;
      const qsizetype prev_i = inner.indexOf(QStringLiteral("class=\"navprev\""));
      if (prev_i >= 0) {
        qsizetype a0 = inner.lastIndexOf(QLatin1Char('<'), prev_i);
        qsizetype a1 = inner.indexOf(QStringLiteral("</a>"), prev_i);
        if (a0 >= 0 && a1 >= 0) {
          prev_html = inner.mid(a0, a1 + 4 - a0);
        }
      }
      const qsizetype next_i = inner.indexOf(QStringLiteral("class=\"navnext\""));
      if (next_i >= 0) {
        qsizetype a0 = inner.lastIndexOf(QLatin1Char('<'), next_i);
        qsizetype a1 = inner.indexOf(QStringLiteral("</a>"), next_i);
        if (a0 >= 0 && a1 >= 0) {
          next_html = inner.mid(a0, a1 + 4 - a0);
        }
      }

      out += QStringLiteral(
          "<table class=\"pager\" width=\"100%\" cellspacing=\"0\" "
          "cellpadding=\"0\"><tr>"
          "<td class=\"pager-prev\">");
      out += prev_html;
      out += QStringLiteral("</td><td class=\"pager-next\">");
      out += next_html;
      out += QStringLiteral("</td></tr></table>");
      pos = close + 6;
    }
    html = out;
  }

  // 5) звичайні таблиці → class="grid" (не чіпати pager)
  {
    QString out;
    out.reserve(html.size() + 64);
    qsizetype pos = 0;
    while (true) {
      const qsizetype open = html.indexOf(QStringLiteral("<table"), pos);
      if (open < 0) {
        out += html.mid(pos);
        break;
      }
      out += html.mid(pos, open - pos);
      const qsizetype tag_end = html.indexOf(QLatin1Char('>'), open);
      if (tag_end < 0) {
        out += html.mid(open);
        break;
      }
      const QString tag = html.mid(open, tag_end - open + 1);
      if (tag.contains(QStringLiteral("class="))) {
        out += tag;
      } else {
        out += QStringLiteral("<table class=\"grid\"");
        out += tag.mid(6);
      }
      pos = tag_end + 1;
    }
    html = out;
  }

  return html;
}

HelpBrowser::HelpBrowser(QWidget *parent) : QTextBrowser(parent) {}

void HelpBrowser::setCodeBlockImages(QVector<QImage> images) {
  codeImages_ = std::move(images);
}

QVariant HelpBrowser::loadResource(int type, const QUrl &name) {
  if (type == QTextDocument::ImageResource) {
    const QString s = name.toString();
    if (s.startsWith(QLatin1String("codeblock:"))) {
      bool ok = false;
      const int index = s.mid(10).toInt(&ok);
      if (ok && index >= 0 && index < codeImages_.size()) {
        return codeImages_.at(index);
      }
    }
  }
  return QTextBrowser::loadResource(type, name);
}

QImage HelpDialog::renderCodeCard(const QString &plain_code, qreal dpr) const {
  constexpr int kWidth = 640;
  constexpr int kRadius = 12;
  constexpr int kPadX = 22;
  constexpr int kPadY = 18;
  constexpr int kHeaderH = 38;
  constexpr int kLineH = 22; // чіткі окремі рядки коду

  QFont mono(QStringLiteral("JetBrains Mono"), 12);
  if (!mono.exactMatch()) {
    mono = QFont(QStringLiteral("Noto Sans Mono"), 12);
  }
  if (!mono.exactMatch()) {
    mono = QFont(QStringLiteral("DejaVu Sans Mono"), 12);
  }

  // Малюємо по рядках (не через HTML) — інакше переноси зникають.
  const QStringList lines = plain_code.split(QLatin1Char('\n'));
  const int text_h = qMax(kLineH, static_cast<int>(lines.size()) * kLineH);
  const int height = kHeaderH + kPadY + text_h + kPadY;

  QImage img(qRound(kWidth * dpr), qRound(height * dpr),
             QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);

  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  const QRectF card(0.5, 0.5, kWidth - 1.0, height - 1.0);
  QPainterPath path;
  path.addRoundedRect(card, kRadius, kRadius);

  p.fillPath(path, QColor(0x0d1117));

  p.save();
  p.setClipPath(path);
  p.fillRect(QRectF(0, 0, kWidth, kHeaderH), QColor(0x161b22));
  p.setPen(QColor(0x30363d));
  p.drawLine(QPointF(0, kHeaderH - 0.5), QPointF(kWidth, kHeaderH - 0.5));
  p.restore();

  p.setPen(QPen(QColor(0x30363d), 1.0));
  p.setBrush(Qt::NoBrush);
  p.drawPath(path);

  QFont headerFont = mono;
  headerFont.setPointSize(10);
  headerFont.setWeight(QFont::DemiBold);
  p.setFont(headerFont);
  p.setPen(QColor(0x8b949e));
  p.drawText(QRectF(kPadX, 0, kWidth / 2.0, kHeaderH),
             Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("calx"));
  p.setPen(QColor(0x58a6ff));
  p.drawText(QRectF(kWidth / 2.0, 0, kWidth / 2.0 - kPadX, kHeaderH),
             Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("Копіювати"));

  p.setFont(mono);
  QFontMetrics fm(mono);
  const int max_text_w = kWidth - 2 * kPadX;
  qreal y = kHeaderH + kPadY + fm.ascent();

  for (const QString &line : lines) {
    qreal x = kPadX;
    for (const HighlightSpan &span : highlightCalxSpans(line)) {
      if (span.text.isEmpty()) {
        continue;
      }
      p.setPen(span.color);
      // якщо рядок довший за картку — обрізаємо хвіст
      QString piece = span.text;
      int adv = fm.horizontalAdvance(piece);
      if (x + adv > kPadX + max_text_w) {
        while (!piece.isEmpty() &&
               x + fm.horizontalAdvance(piece) > kPadX + max_text_w) {
          piece.chop(1);
        }
        if (piece.isEmpty()) {
          break;
        }
        adv = fm.horizontalAdvance(piece);
      }
      p.drawText(QPointF(x, y), piece);
      x += adv;
      if (x >= kPadX + max_text_w) {
        break;
      }
    }
    y += kLineH;
  }

  p.end();
  return img;
}


HelpDialog::HelpDialog(QWidget *parent) : QWidget(parent, Qt::Window) {
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowFlag(Qt::Tool, false);
  setWindowFlag(Qt::Window, true);
  finishSetup(0);
}

void HelpDialog::openSection(const QString &section_title_or_id) {
  int row = indexById(section_title_or_id);
  if (row < 0) {
    row = indexByTitle(section_title_or_id);
  }
  if (row < 0) {
    row = 0;
  }
  if (nav_) {
    nav_->setCurrentRow(row);
  }
  show();
  raise();
  activateWindow();
}

void HelpDialog::closeEvent(QCloseEvent *event) {
  hide();
  event->ignore();
}

int HelpDialog::sectionIndexByTitle(const QString &title) {
  const int by_title = indexByTitle(title);
  if (by_title >= 0) {
    return by_title;
  }
  const int by_id = indexById(title);
  return by_id >= 0 ? by_id : 0;
}

void HelpDialog::onAnchorClicked(const QUrl &url) {
  const QString raw = url.toString();
  if (raw.startsWith(QLatin1String("copy:"))) {
    bool ok = false;
    const int idx = raw.mid(5).toInt(&ok);
    if (ok && idx >= 0 && idx < copySnippets_.size()) {
      QGuiApplication::clipboard()->setText(copySnippets_.at(idx));
      if (copyHint_) {
        copyHint_->setText(QStringLiteral("Скопійовано в буфер обміну"));
        QTimer::singleShot(1800, this, [this] {
          if (copyHint_) {
            copyHint_->clear();
          }
        });
      }
    }
    return;
  }
  if (raw.startsWith(QLatin1String("help:"))) {
    QString target = raw.mid(5);
    if (target.startsWith(QLatin1String("//"))) {
      target = target.mid(2);
    }
    QString frag;
    const auto hash = target.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
      frag = target.mid(hash + 1);
      target = target.left(hash);
    }
    if (frag.isEmpty() && !url.fragment().isEmpty()) {
      frag = url.fragment();
    }
    pendingAnchor_ = frag;
    openSection(QUrl::fromPercentEncoding(target.toUtf8()));
    return;
  }
  const QString frag = url.fragment();
  if (!frag.isEmpty()) {
    browser_->scrollToAnchor(frag);
  }
}

void HelpDialog::finishSetup(int initial_row) {
  setWindowTitle(QStringLiteral("Довідка — calx IDE"));
  resize(1080, 760);
  setMinimumSize(780, 520);

  QPalette dlg = palette();
  dlg.setColor(QPalette::Window, QColor(0x1b1f27));
  dlg.setColor(QPalette::WindowText, QColor(0xe8eaed));
  dlg.setColor(QPalette::Base, QColor(0xf7f8fa));
  dlg.setColor(QPalette::Text, QColor(0x1c2128));
  dlg.setColor(QPalette::Button, QColor(0x2c3340));
  dlg.setColor(QPalette::ButtonText, QColor(0xe8eaed));
  dlg.setColor(QPalette::Highlight, QColor(0x3d7ea6));
  dlg.setColor(QPalette::HighlightedText, QColor(0xffffff));
  setPalette(dlg);

  auto *brand = new QLabel(QStringLiteral("calx"), this);
  {
    QFont f = brand->font();
    f.setPointSize(qMax(11, f.pointSize() + 1));
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    brand->setFont(f);
  }
  brand->setStyleSheet(
      QStringLiteral("color:#8eb6d3; padding: 2px 4px 0 4px;"));

  auto *title = new QLabel(QStringLiteral("Довідка"), this);
  {
    QFont f = title->font();
    f.setPointSize(qMax(13, f.pointSize() + 3));
    f.setBold(true);
    title->setFont(f);
  }
  title->setStyleSheet(
      QStringLiteral("color:#f0f2f5; padding: 0 4px 10px 4px;"));

  nav_ = new QListWidget(this);
  nav_->setFixedWidth(228);
  nav_->setStyleSheet(QStringLiteral(
      "QListWidget {"
      "  background:#151920; color:#c5ccd6; border:none;"
      "  outline:none; font-size:13px; padding:4px 0 8px;"
      "}"
      "QListWidget::item {"
      "  padding:7px 14px; margin:1px 8px; border-radius:6px;"
      "}"
      "QListWidget::item:selected {"
      "  background:#2f6f94; color:#ffffff; font-weight:600;"
      "}"
      "QListWidget::item:hover:!selected {"
      "  background:#252b36;"
      "}"));
  for (const HelpSection &section : kSections) {
    nav_->addItem(QString::fromUtf8(section.title));
  }

  browser_ = new HelpBrowser(this);
  browser_->setOpenExternalLinks(false);
  browser_->setOpenLinks(false);
  browser_->setFrameShape(QFrame::NoFrame);
  {
    QPalette pal = browser_->palette();
    pal.setColor(QPalette::Base, QColor(0xf7f8fa));
    pal.setColor(QPalette::Text, QColor(0x1c2128));
    pal.setColor(QPalette::WindowText, QColor(0x1c2128));
    pal.setColor(QPalette::Link, QColor(0x1a6fa8));
    pal.setColor(QPalette::LinkVisited, QColor(0x1a6fa8));
    browser_->setPalette(pal);
    QFont font = browser_->font();
    font.setPointSize(qMax(11, font.pointSize() + 1));
    browser_->setFont(font);
  }

  copyHint_ = new QLabel(this);
  copyHint_->setStyleSheet(
      QStringLiteral("color:#8eb6d3; padding-left:4px; font-size:12px;"));

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрити"));
  buttons->setStyleSheet(
      QStringLiteral("QPushButton { background:#2c3340; color:#e8eaed; "
                     "border:1px solid #3d4656;"
                     "  padding:7px 18px; border-radius:5px; }"
                     "QPushButton:hover { background:#3a4454; }"));

  connect(nav_, &QListWidget::currentRowChanged, this,
          &HelpDialog::showSection);
  connect(browser_, &QTextBrowser::anchorClicked, this,
          &HelpDialog::onAnchorClicked);
  connect(buttons, &QDialogButtonBox::rejected, this, &HelpDialog::hide);
  connect(buttons, &QDialogButtonBox::accepted, this, &HelpDialog::hide);

  auto *left = new QWidget(this);
  auto *left_layout = new QVBoxLayout(left);
  left_layout->setContentsMargins(12, 14, 6, 12);
  left_layout->setSpacing(0);
  left_layout->addWidget(brand);
  left_layout->addWidget(title);
  left_layout->addWidget(nav_, 1);

  auto *footer = new QWidget(this);
  auto *footer_layout = new QHBoxLayout(footer);
  footer_layout->setContentsMargins(0, 0, 0, 0);
  footer_layout->addWidget(copyHint_, 1);
  footer_layout->addWidget(buttons, 0, Qt::AlignRight);

  auto *right = new QWidget(this);
  auto *right_layout = new QVBoxLayout(right);
  right_layout->setContentsMargins(6, 12, 12, 12);
  right_layout->setSpacing(8);
  right_layout->addWidget(browser_, 1);
  right_layout->addWidget(footer);

  auto *split = new QSplitter(Qt::Horizontal, this);
  split->addWidget(left);
  split->addWidget(right);
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  split->setChildrenCollapsible(false);
  split->setStyleSheet(
      QStringLiteral("QSplitter::handle { background:#0f1218; width:1px; }"));

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
  const QString body = enrichHtml(QString::fromUtf8(section.html));
  browser_->setCodeBlockImages(codeImages_);
  browser_->setHtml(pageChrome(body));
  if (copyHint_) {
    copyHint_->clear();
  }
  if (!pendingAnchor_.isEmpty()) {
    browser_->scrollToAnchor(pendingAnchor_);
    pendingAnchor_.clear();
  } else {
    browser_->moveCursor(QTextCursor::Start);
  }
}
