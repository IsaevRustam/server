# Краткий экскурс
в функции processconnection идет проверка на cgi, если cgi, то там есть "?" QUERY_STRING так сказать, то работаем одним способом, если нет то другим

FillEnv
env[0] = реквест, строка которую просто сервер прислал, полностью, она там делится на вектор, а вот то, что было до деления на вектора будет в env[0]
в остальных env[i] лежит всё прилегающее: адрес, имя, порт и прочее

в файле cgi.cpp используются функции getenv("...") она достает всё, что после равно будет и так со всеми(адрес, имя и тд), там дальше просто стркопи и прочее, присваивания обычные 
в основной программе делается любимый fork() и в сыне заполнить весь этот массив. создаем на запись файл временный и dup2, весь вывод идёт в этот файл. для проверки на ошибки есть отдельная функция для ошибок.
в отце он ждет статус и если все хорошо открываем этот временный файл на чтение, разбиваем на вектора и послыаем как обычный файл.

работает в браузере: 
http://127.0.0.1:8080/
http://127.0.0.1:8080/cgi-bin/cgi?Name=R&Surname=I&TG=RI
http://127.0.0.1:8080/qwer.txt

выдает 404 на "ошибочных запросах"
на:

http://127.0.0.1:8080/asdfasdf
а еще на:

http://127.0.0.1:8080/qwerqwer
возможно и на:

http://127.0.0.1:8080/avsvfrwwwe, но надо уточнять конечно, да

////////////
По проге client.cpp

бОльшая часть функций взяты с первой и второй консультаций

HttpHeader
name это get, а value это всё остальное, то есть мы тут разделяем, parseheader собственно это и делает
он принимает стринг, достает первый гет, засовывает его в нейм, а всё остальное это value, там есть дополнительные методы,
но они тривиальны по типу getname и getstring, главное было не забыть указать const, на этом "особенности" header'а закончились

HttpRequest
Это то что у нас клиент отсылает, в _lines мы прописываем что хотим отправить, проблемы возникли именно с этой функцией
не получалось ничего отправить, проблема была в том, что не было дописано HTTP/1.1

HttpResponse
Нам присылается строка какая-то с сервера, мы её разбиваем на векторы
функцию для векторов еще на семинарах обговаривали в начале семестра(вроде) и очень много было сказано на консультации
и тогда нулевой вектор это response, потом идут подряд какие-то другие "other" это всё так же взято с консультации
и дальше функция простого заполнения, принимается вектор и заполняется, пока не встречается пустая строка, ...empty() библиотечная функция

ClientConnection
создаем клиентсокет, потом сокетадрес, потом подсоединяемся коннектом, создаем HttpRequest, переобразовываем его в string
чтобы отослать его write'ом на сервер, отсылаем на сервер, сервер нам что-то отвечает, читаем, в методичке Столярова 
написано что не понятно сколько раз читать, потому что читать можно сколько угодно, после этого добавляем это во временную строчку
функцией splitlines разбиваем на вектора, это с консультации взято, заполняем HttpResponse

По прогу jjj2.cpp это сам сервер

понять как работает было сложнее, чем писать

TURNON
Самый банальный loop в виде turnon который просто вызывается мейном сразу, создаем сокетадрес, сервер сокетный
биндим его, у меня много времени ушло на то, чтобы написать свою функцию bind, а как оказалось можно использовать библиотечную
используем listen и запускаем главный цикл, в нем мы каждый раз создаем сокетадрес и запихиваем его в processconnection
accept возвращает сам серверсокет, ну то есть сокет того к которому подключился и мы его запихиваем дальше, дальше идём в processconnection

processconnection
каждый раз создается connectedsoket от клиентского дескриптора, читаем строку, разбиваем на векторы, у нас в нулевом лежит get потом используем getpath что бы достать что после get и что перед http, в случае с клиентом это qwer.txt
после этого нужно банально открыть файл который мы получили, открываем index.html и ИНАЧЕ открываем то, что у нас в path, который мы достали уже
если меньше нуля fd то открываем 404 и пишем что всё плохо, открываем 404.html, дальше функция writefile

writefile
самое главное что есть в этой функции это contentlenght, это длина того, что отсылается, там по одному биту считывается с файла
дальше выделяется память для буфера, потому что send работает для char*, чтобы писать в сокет, библиотечная функция, неинтересно с ней, копируем 
в буф строку вот эту, функций c_str преобразоываеем string в char*, дальше банально копируем и берем длину, потому что send еще и длину требует для работы
дальше просто считываем из файла в буфер и отправляем этот буфер, то есть мы такими пачками отсылаем 
