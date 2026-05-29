/**
 * Gateway — Main Application
 * XPDevs — https://xpdevs.github.io
 */
(function() {
    'use strict';

    const $ = id => document.getElementById(id);

    // ======================== TRANSLATIONS ========================

    const LANG = {
        en: { about:'About', images:'Images', searchPlaceholder:'Search Gateway or type a URL', gatewaySearch:'Gateway Search', feelingLucky:"I'm Feeling Lucky", unitedKingdom:'United Kingdom', advertising:'Advertising', business:'Business', howSearchWorks:'How Search works', privacy:'Privacy', terms:'Terms', settings:'Settings', all:'All', showMoreResults:'Show more results', noResultsFound:'No results found', tryDifferentKeywords:'Try different keywords or check your spelling', didYouMean:'Did you mean:', quickAnswer:'Quick Answer', searchingMultipleSources:'Searching multiple sources...', connectionError:'Connection error. Please check your internet and try again.', tagline:'Faster than Google. Same results. No API. No index.', resultsStats:'About {count} results — No API. No index. Faster.', settingsTitle:'Settings', language:'Language', darkMode:'Dark mode', close:'Close', wikipedia:'Wikipedia' },
        es: { about:'Acerca de', images:'Imágenes', searchPlaceholder:'Buscar en Gateway o escribe una URL', gatewaySearch:'Buscar con Gateway', feelingLucky:'Voy a tener suerte', unitedKingdom:'Reino Unido', advertising:'Publicidad', business:'Negocios', howSearchWorks:'Cómo funciona la Búsqueda', privacy:'Privacidad', terms:'Términos', settings:'Configuración', all:'Todo', showMoreResults:'Mostrar más resultados', noResultsFound:'No se encontraron resultados', tryDifferentKeywords:'Prueba con otras palabras o revisa la ortografía', didYouMean:'Quizás quisiste decir:', quickAnswer:'Respuesta rápida', searchingMultipleSources:'Buscando en múltiples fuentes...', connectionError:'Error de conexión. Verifica tu internet e inténtalo de nuevo.', tagline:'Más rápido que Google. Mismos resultados. Sin API. Sin índice.', resultsStats:'Aprox. {count} resultados — Sin API. Sin índice. Más rápido.', settingsTitle:'Configuración', language:'Idioma', darkMode:'Modo oscuro', close:'Cerrar', wikipedia:'Wikipedia' },
        fr: { about:'À propos', images:'Images', searchPlaceholder:'Rechercher sur Gateway ou saisir une URL', gatewaySearch:'Recherche Gateway', feelingLucky:'J\'ai de la chance', unitedKingdom:'Royaume-Uni', advertising:'Publicité', business:'Entreprises', howSearchWorks:'Fonctionnement de la Recherche', privacy:'Confidentialité', terms:'Conditions', settings:'Paramètres', all:'Tout', showMoreResults:'Plus de résultats', noResultsFound:'Aucun résultat trouvé', tryDifferentKeywords:'Essayez différents mots-clés ou vérifiez l\'orthographe', didYouMean:'Vouliez-vous dire :', quickAnswer:'Réponse rapide', searchingMultipleSources:'Recherche multi-sources...', connectionError:'Erreur de connexion. Vérifiez votre connexion et réessayez.', tagline:'Plus rapide que Google. Mêmes résultats. Sans API. Sans index.', resultsStats:'Environ {count} résultats — Sans API. Sans index. Plus rapide.', settingsTitle:'Paramètres', language:'Langue', darkMode:'Mode sombre', close:'Fermer', wikipedia:'Wikipédia' },
        de: { about:'Über uns', images:'Bilder', searchPlaceholder:'Gateway durchsuchen oder URL eingeben', gatewaySearch:'Gateway-Suche', feelingLucky:'Auf gut Glück', unitedKingdom:'Vereinigtes Königreich', advertising:'Werbung', business:'Unternehmen', howSearchWorks:'So funktioniert die Suche', privacy:'Datenschutz', terms:'AGB', settings:'Einstellungen', all:'Alle', showMoreResults:'Weitere Ergebnisse', noResultsFound:'Keine Ergebnisse gefunden', tryDifferentKeywords:'Versuchen Sie andere Suchbegriffe oder überprüfen Sie die Rechtschreibung', didYouMean:'Meinten Sie:', quickAnswer:'Kurze Antwort', searchingMultipleSources:'Durchsuche mehrere Quellen...', connectionError:'Verbindungsfehler. Bitte Internet prüfen und erneut versuchen.', tagline:'Schneller als Google. Gleiche Ergebnisse. Ohne API. Ohne Index.', resultsStats:'Ca. {count} Ergebnisse — Ohne API. Ohne Index. Schneller.', settingsTitle:'Einstellungen', language:'Sprache', darkMode:'Dunkelmodus', close:'Schließen', wikipedia:'Wikipedia' },
        it: { about:'Informazioni', images:'Immagini', searchPlaceholder:'Cerca su Gateway o digita un URL', gatewaySearch:'Cerca con Gateway', feelingLucky:'Mi sento fortunato', unitedKingdom:'Regno Unito', advertising:'Pubblicità', business:'Business', howSearchWorks:'Come funziona la Ricerca', privacy:'Privacy', terms:'Termini', settings:'Impostazioni', all:'Tutto', showMoreResults:'Mostra altri risultati', noResultsFound:'Nessun risultato trovato', tryDifferentKeywords:'Prova con parole diverse o controlla l\'ortografia', didYouMean:'Forse cercavi:', quickAnswer:'Risposta rapida', searchingMultipleSources:'Ricerca in più fonti...', connectionError:'Errore di connessione. Controlla la connessione e riprova.', tagline:'Più veloce di Google. Stessi risultati. Senza API. Senza indice.', resultsStats:'Circa {count} risultati — Senza API. Senza indice. Più veloce.', settingsTitle:'Impostazioni', language:'Lingua', darkMode:'Modalità scura', close:'Chiudi', wikipedia:'Wikipedia' },
        pt: { about:'Sobre', images:'Imagens', searchPlaceholder:'Pesquisar Gateway ou digitar URL', gatewaySearch:'Pesquisa Gateway', feelingLucky:'Estou com sorte', unitedKingdom:'Reino Unido', advertising:'Publicidade', business:'Negócios', howSearchWorks:'Como funciona a Pesquisa', privacy:'Privacidade', terms:'Termos', settings:'Configurações', all:'Tudo', showMoreResults:'Mostrar mais resultados', noResultsFound:'Nenhum resultado encontrado', tryDifferentKeywords:'Tente palavras-chave diferentes ou verifique a ortografia', didYouMean:'Você quis dizer:', quickAnswer:'Resposta rápida', searchingMultipleSources:'Pesquisando em múltiplas fontes...', connectionError:'Erro de conexão. Verifique sua internet e tente novamente.', tagline:'Mais rápido que o Google. Mesmos resultados. Sem API. Sem índice.', resultsStats:'Aprox. {count} resultados — Sem API. Sem índice. Mais rápido.', settingsTitle:'Configurações', language:'Idioma', darkMode:'Modo escuro', close:'Fechar', wikipedia:'Wikipédia' },
        ru: { about:'О нас', images:'Картинки', searchPlaceholder:'Поиск в Gateway или введите URL', gatewaySearch:'Поиск Gateway', feelingLucky:'Мне повезёт', unitedKingdom:'Великобритания', advertising:'Реклама', business:'Бизнес', howSearchWorks:'Как работает поиск', privacy:'Конфиденциальность', terms:'Условия', settings:'Настройки', all:'Все', showMoreResults:'Показать больше', noResultsFound:'Ничего не найдено', tryDifferentKeywords:'Попробуйте другие слова или проверьте орфографию', didYouMean:'Возможно, вы имели в виду:', quickAnswer:'Быстрый ответ', searchingMultipleSources:'Поиск по нескольким источникам...', connectionError:'Ошибка подключения. Проверьте интернет и повторите попытку.', tagline:'Быстрее Google. Те же результаты. Без API. Без индекса.', resultsStats:'Примерно {count} результатов — Без API. Без индекса. Быстрее.', settingsTitle:'Настройки', language:'Язык', darkMode:'Тёмная тема', close:'Закрыть', wikipedia:'Википедия' },
        ja: { about:'概要', images:'画像', searchPlaceholder:'Gatewayを検索、またはURLを入力', gatewaySearch:'Gateway検索', feelingLucky:'I\'m Feeling Lucky', unitedKingdom:'イギリス', advertising:'広告', business:'ビジネス', howSearchWorks:'検索の仕組み', privacy:'プライバシー', terms:'利用規約', settings:'設定', all:'すべて', showMoreResults:'さらに表示', noResultsFound:'結果が見つかりませんでした', tryDifferentKeywords:'別のキーワードを試すか、スペルを確認してください', didYouMean:'もしかして:', quickAnswer:'クイックアンサー', searchingMultipleSources:'複数のソースを検索中...', connectionError:'接続エラーです。インターネット接続を確認してもう一度お試しください。', tagline:'Googleより高速。同じ結果。API不要。インデックス不要。', resultsStats:'約{count}件 — API不要。インデックス不要。より高速。', settingsTitle:'設定', language:'言語', darkMode:'ダークモード', close:'閉じる', wikipedia:'ウィキペディア' },
        'zh-CN': { about:'关于', images:'图片', searchPlaceholder:'搜索 Gateway 或输入网址', gatewaySearch:'Gateway 搜索', feelingLucky:'手气不错', unitedKingdom:'英国', advertising:'广告', business:'商务', howSearchWorks:'搜索工作原理', privacy:'隐私', terms:'条款', settings:'设置', all:'全部', showMoreResults:'显示更多结果', noResultsFound:'未找到结果', tryDifferentKeywords:'尝试不同的关键词或检查拼写', didYouMean:'您是不是要找：', quickAnswer:'快速解答', searchingMultipleSources:'正在搜索多个来源...', connectionError:'连接错误。请检查网络后重试。', tagline:'比谷歌更快。相同结果。无需API。无需索引。', resultsStats:'约{count}条结果 — 无需API。无需索引。更快。', settingsTitle:'设置', language:'语言', darkMode:'深色模式', close:'关闭', wikipedia:'维基百科' },
        'zh-TW': { about:'關於', images:'圖片', searchPlaceholder:'搜尋 Gateway 或輸入網址', gatewaySearch:'Gateway 搜尋', feelingLucky:'好手氣', unitedKingdom:'英國', advertising:'廣告', business:'商務', howSearchWorks:'搜尋運作方式', privacy:'隱私權', terms:'條款', settings:'設定', all:'全部', showMoreResults:'顯示更多結果', noResultsFound:'找不到結果', tryDifferentKeywords:'請嘗試不同的關鍵字或檢查拼寫', didYouMean:'您是不是要找：', quickAnswer:'快速解答', searchingMultipleSources:'正在搜尋多個來源...', connectionError:'連線錯誤。請檢查網路後重試。', tagline:'比 Google 更快。相同結果。無需 API。無需索引。', resultsStats:'約{count}項結果 — 無需 API。無需索引。更快。', settingsTitle:'設定', language:'語言', darkMode:'深色模式', close:'關閉', wikipedia:'維基百科' },
        ko: { about:'정보', images:'이미지', searchPlaceholder:'Gateway 검색 또는 URL 입력', gatewaySearch:'Gateway 검색', feelingLucky:'행운을 빌어요', unitedKingdom:'영국', advertising:'광고', business:'비즈니스', howSearchWorks:'검색 작동 방식', privacy:'개인정보', terms:'약관', settings:'설정', all:'전체', showMoreResults:'더 많은 결과 보기', noResultsFound:'검색 결과가 없습니다', tryDifferentKeywords:'다른 키워드를 시도하거나 철자를 확인하세요', didYouMean:'혹시 찾으시는 것이:', quickAnswer:'빠른 답변', searchingMultipleSources:'여러 소스 검색 중...', connectionError:'연결 오류입니다. 인터넷을 확인하고 다시 시도하세요.', tagline:'Google보다 빠름. 동일한 결과. API 불필요. 인덱스 불필요.', resultsStats:'약 {count}개 결과 — API 불필요. 인덱스 불필요. 더 빠름.', settingsTitle:'설정', language:'언어', darkMode:'다크 모드', close:'닫기', wikipedia:'위키백과' },
        ar: { about:'حول', images:'صور', searchPlaceholder:'ابحث في Gateway أو أدخل رابطاً', gatewaySearch:'بحث Gateway', feelingLucky:'أنا محظوظ', unitedKingdom:'المملكة المتحدة', advertising:'إعلانات', business:'أعمال', howSearchWorks:'كيف يعمل البحث', privacy:'خصوصية', terms:'الشروط', settings:'إعدادات', all:'الكل', showMoreResults:'عرض المزيد من النتائج', noResultsFound:'لم يتم العثور على نتائج', tryDifferentKeywords:'جرّب كلمات مختلفة أو تحقق من الإملاء', didYouMean:'هل تقصد:', quickAnswer:'إجابة سريعة', searchingMultipleSources:'جاري البحث في مصادر متعددة...', connectionError:'خطأ في الاتصال. تحقق من اتصالك بالإنترنت وحاول مرة أخرى.', tagline:'أسرع من Google. نفس النتائج. بدون API. بدون فهرس.', resultsStats:'حوالي {count} نتيجة — بدون API. بدون فهرس. أسرع.', settingsTitle:'الإعدادات', language:'اللغة', darkMode:'الوضع الداكن', close:'إغلاق', wikipedia:'ويكيبيديا' },
        hi: { about:'बारे में', images:'चित्र', searchPlaceholder:'Gateway में खोजें या URL टाइप करें', gatewaySearch:'Gateway खोज', feelingLucky:'मैं भाग्यशाली हूँ', unitedKingdom:'यूनाइटेड किंगडम', advertising:'विज्ञापन', business:'व्यवसाय', howSearchWorks:'खोज कैसे काम करती है', privacy:'गोपनीयता', terms:'शर्तें', settings:'सेटिंग्स', all:'सभी', showMoreResults:'और परिणाम दिखाएँ', noResultsFound:'कोई परिणाम नहीं मिला', tryDifferentKeywords:'अलग कीवर्ड आज़माएँ या वर्तनी जाँचें', didYouMean:'क्या आप यह कहना चाह रहे थे:', quickAnswer:'त्वरित उत्तर', searchingMultipleSources:'कई स्रोतों में खोज रहे हैं...', connectionError:'कनेक्शन त्रुटि। कृपया अपना इंटरनेट जाँचें और पुनः प्रयास करें।', tagline:'Google से तेज़। वही परिणाम। कोई API नहीं। कोई इंडेक्स नहीं।', resultsStats:'लगभग {count} परिणाम — कोई API नहीं। कोई इंडेक्स नहीं। तेज़।', settingsTitle:'सेटिंग्स', language:'भाषा', darkMode:'डार्क मोड', close:'बंद करें', wikipedia:'विकिपीडिया' },
        bn: { about:'সম্পর্কে', images:'ছবি', searchPlaceholder:'Gateway-এ অনুসন্ধান করুন বা URL লিখুন', gatewaySearch:'Gateway অনুসন্ধান', feelingLucky:'আমি ভাগ্যবান', unitedKingdom:'যুক্তরাজ্য', advertising:'বিজ্ঞাপন', business:'ব্যবসা', howSearchWorks:'কিভাবে অনুসন্ধান কাজ করে', privacy:'গোপনীয়তা', terms:'শর্তাবলী', settings:'সেটিংস', all:'সব', showMoreResults:'আরও ফলাফল দেখান', noResultsFound:'কোনো ফলাফল পাওয়া যায়নি', tryDifferentKeywords:'ভিন্ন শব্দ ব্যবহার করুন বা বানান পরীক্ষা করুন', didYouMean:'আপনি কি বোঝাতে চেয়েছেন:', quickAnswer:'দ্রুত উত্তর', searchingMultipleSources:'একাধিক উৎসে অনুসন্ধান করা হচ্ছে...', connectionError:'সংযোগ ত্রুটি। আপনার ইন্টারনেট পরীক্ষা করে আবার চেষ্টা করুন।', tagline:'Google-এর চেয়ে দ্রুত। একই ফলাফল। কোনো API নেই। কোনো ইনডেক্স নেই।', resultsStats:'প্রায় {count}টি ফলাফল — কোনো API নেই। কোনো ইনডেক্স নেই। দ্রুততর।', settingsTitle:'সেটিংস', language:'ভাষা', darkMode:'ডার্ক মোড', close:'বন্ধ করুন', wikipedia:'উইকিপিডিয়া' },
        tr: { about:'Hakkında', images:'Görseller', searchPlaceholder:'Gateway\'de ara veya URL yaz', gatewaySearch:'Gateway Arama', feelingLucky:'Şanslıyım', unitedKingdom:'Birleşik Krallık', advertising:'Reklam', business:'İşletme', howSearchWorks:'Arama nasıl çalışır', privacy:'Gizlilik', terms:'Şartlar', settings:'Ayarlar', all:'Tümü', showMoreResults:'Daha fazla sonuç göster', noResultsFound:'Sonuç bulunamadı', tryDifferentKeywords:'Farklı anahtar kelimeler deneyin veya yazımı kontrol edin', didYouMean:'Bunu mu demek istediniz:', quickAnswer:'Hızlı Cevap', searchingMultipleSources:'Birden çok kaynak taranıyor...', connectionError:'Bağlantı hatası. Lütfen internetinizi kontrol edip tekrar deneyin.', tagline:'Google\'dan daha hızlı. Aynı sonuçlar. API yok. Dizin yok.', resultsStats:'Yaklaşık {count} sonuç — API yok. Dizin yok. Daha hızlı.', settingsTitle:'Ayarlar', language:'Dil', darkMode:'Karanlık mod', close:'Kapat', wikipedia:'Vikipedi' },
        nl: { about:'Over ons', images:'Afbeeldingen', searchPlaceholder:'Zoek op Gateway of typ een URL', gatewaySearch:'Gateway Zoeken', feelingLucky:'Gelukzoeker', unitedKingdom:'Verenigd Koninkrijk', advertising:'Adverteren', business:'Zakelijk', howSearchWorks:'Hoe zoeken werkt', privacy:'Privacy', terms:'Voorwaarden', settings:'Instellingen', all:'Alles', showMoreResults:'Meer resultaten', noResultsFound:'Geen resultaten gevonden', tryDifferentKeywords:'Probeer andere zoekwoorden of controleer de spelling', didYouMean:'Bedoelde u:', quickAnswer:'Snel antwoord', searchingMultipleSources:'Meerdere bronnen doorzoeken...', connectionError:'Verbindingsfout. Controleer uw internet en probeer het opnieuw.', tagline:'Sneller dan Google. Zelfde resultaten. Geen API. Geen index.', resultsStats:'Ongeveer {count} resultaten — Geen API. Geen index. Sneller.', settingsTitle:'Instellingen', language:'Taal', darkMode:'Donkere modus', close:'Sluiten', wikipedia:'Wikipedia' },
        pl: { about:'O nas', images:'Obrazy', searchPlaceholder:'Szukaj w Gateway lub wpisz URL', gatewaySearch:'Szukaj w Gateway', feelingLucky:'Szczęściarz', unitedKingdom:'Wielka Brytania', advertising:'Reklama', business:'Firmy', howSearchWorks:'Jak działa wyszukiwanie', privacy:'Prywatność', terms:'Warunki', settings:'Ustawienia', all:'Wszystkie', showMoreResults:'Pokaż więcej wyników', noResultsFound:'Brak wyników', tryDifferentKeywords:'Spróbuj innych słów kluczowych lub sprawdź pisownię', didYouMean:'Czy chodziło Ci o:', quickAnswer:'Szybka odpowiedź', searchingMultipleSources:'Przeszukiwanie wielu źródeł...', connectionError:'Błąd połączenia. Sprawdź internet i spróbuj ponownie.', tagline:'Szybsze niż Google. Te same wyniki. Bez API. Bez indeksu.', resultsStats:'Około {count} wyników — Bez API. Bez indeksu. Szybciej.', settingsTitle:'Ustawienia', language:'Język', darkMode:'Tryb ciemny', close:'Zamknij', wikipedia:'Wikipedia' },
        sv: { about:'Om', images:'Bilder', searchPlaceholder:'Sök på Gateway eller skriv en URL', gatewaySearch:'Gateway-sökning', feelingLucky:'Jag känner mig turlig', unitedKingdom:'Storbritannien', advertising:'Annonsering', business:'Företag', howSearchWorks:'Så fungerar sökning', privacy:'Integritet', terms:'Villkor', settings:'Inställningar', all:'Alla', showMoreResults:'Visa fler resultat', noResultsFound:'Inga resultat hittades', tryDifferentKeywords:'Prova andra sökord eller kontrollera stavningen', didYouMean:'Menade du:', quickAnswer:'Snabbt svar', searchingMultipleSources:'Söker i flera källor...', connectionError:'Anslutningsfel. Kontrollera din internetanslutning och försök igen.', tagline:'Snabbare än Google. Samma resultat. Inget API. Inget index.', resultsStats:'Ungefär {count} resultat — Inget API. Inget index. Snabbare.', settingsTitle:'Inställningar', language:'Språk', darkMode:'Mörkt läge', close:'Stäng', wikipedia:'Wikipedia' },
        da: { about:'Om', images:'Billeder', searchPlaceholder:'Søg på Gateway eller skriv en URL', gatewaySearch:'Gateway-søgning', feelingLucky:'Jeg er heldig', unitedKingdom:'Storbritannien', advertising:'Annoncering', business:'Virksomhed', howSearchWorks:'Sådan fungerer søgning', privacy:'Privatliv', terms:'Vilkår', settings:'Indstillinger', all:'Alle', showMoreResults:'Vis flere resultater', noResultsFound:'Ingen resultater fundet', tryDifferentKeywords:'Prøv andre søgeord eller tjek stavningen', didYouMean:'Mente du:', quickAnswer:'Hurtigt svar', searchingMultipleSources:'Søger i flere kilder...', connectionError:'Forbindelsesfejl. Tjek din internetforbindelse og prøv igen.', tagline:'Hurtigere end Google. Samme resultater. Intet API. Intet indeks.', resultsStats:'Ca. {count} resultater — Intet API. Intet indeks. Hurtigere.', settingsTitle:'Indstillinger', language:'Sprog', darkMode:'Mørk tilstand', close:'Luk', wikipedia:'Wikipedia' },
        fi: { about:'Tietoja', images:'Kuvat', searchPlaceholder:'Hae Gatewaysta tai kirjoita URL', gatewaySearch:'Gateway-haku', feelingLucky:'Minulla on tuuria', unitedKingdom:'Yhdistynyt kuningaskunta', advertising:'Mainonta', business:'Yritykset', howSearchWorks:'Näin haku toimii', privacy:'Yksityisyys', terms:'Ehdot', settings:'Asetukset', all:'Kaikki', showMoreResults:'Näytä lisää tuloksia', noResultsFound:'Ei tuloksia', tryDifferentKeywords:'Kokeile eri hakusanoja tai tarkista oikeinkirjoitus', didYouMean:'Tarkoititko:', quickAnswer:'Pikavastaus', searchingMultipleSources:'Haetaan useista lähteistä...', connectionError:'Yhteysvirhe. Tarkista internetyhteys ja yritä uudelleen.', tagline:'Nopeampi kuin Google. Samat tulokset. Ei APIa. Ei indeksiä.', resultsStats:'Noin {count} tulosta — Ei APIa. Ei indeksiä. Nopeampi.', settingsTitle:'Asetukset', language:'Kieli', darkMode:'Tumma tila', close:'Sulje', wikipedia:'Wikipedia' },
        no: { about:'Om', images:'Bilder', searchPlaceholder:'Søk på Gateway eller skriv en URL', gatewaySearch:'Gateway-søk', feelingLucky:'Jeg er heldig', unitedKingdom:'Storbritannia', advertising:'Annonsering', business:'Bedrifter', howSearchWorks:'Slik fungerer søk', privacy:'Personvern', terms:'Vilkår', settings:'Innstillinger', all:'Alle', showMoreResults:'Vis flere resultater', noResultsFound:'Ingen resultater funnet', tryDifferentKeywords:'Prøv andre søkeord eller sjekk stavemåten', didYouMean:'Mente du:', quickAnswer:'Hurtig svar', searchingMultipleSources:'Søker i flere kilder...', connectionError:'Tilkoblingsfeil. Sjekk internettilkoblingen og prøv igjen.', tagline:'Raskere enn Google. Samme resultater. Uten API. Uten indeks.', resultsStats:'Omtrent {count} resultater — Uten API. Uten indeks. Raskere.', settingsTitle:'Innstillinger', language:'Språk', darkMode:'Mørk modus', close:'Lukk', wikipedia:'Wikipedia' },
        cs: { about:'O nás', images:'Obrázky', searchPlaceholder:'Hledat na Gateway nebo zadat URL', gatewaySearch:'Hledat Gateway', feelingLucky:'Chci mít štěstí', unitedKingdom:'Spojené království', advertising:'Reklama', business:'Firmy', howSearchWorks:'Jak vyhledávání funguje', privacy:'Soukromí', terms:'Smluvní podmínky', settings:'Nastavení', all:'Vše', showMoreResults:'Zobrazit více výsledků', noResultsFound:'Nebyly nalezeny žádné výsledky', tryDifferentKeywords:'Zkuste jiná klíčová slova nebo zkontrolujte pravopis', didYouMean:'Mysleli jste:', quickAnswer:'Rychlá odpověď', searchingMultipleSources:'Vyhledávání ve více zdrojích...', connectionError:'Chyba připojení. Zkontrolujte připojení k internetu a zkuste to znovu.', tagline:'Rychlejší než Google. Stejné výsledky. Bez API. Bez indexu.', resultsStats:'Přibližně {count} výsledků — Bez API. Bez indexu. Rychleji.', settingsTitle:'Nastavení', language:'Jazyk', darkMode:'Tmavý režim', close:'Zavřít', wikipedia:'Wikipedie' },
        ro: { about:'Despre', images:'Imagini', searchPlaceholder:'Căutați pe Gateway sau introduceți o adresă URL', gatewaySearch:'Căutare Gateway', feelingLucky:'Norocos', unitedKingdom:'Regatul Unit', advertising:'Publicitate', business:'Afaceri', howSearchWorks:'Cum funcționează Căutarea', privacy:'Confidențialitate', terms:'Termeni', settings:'Setări', all:'Toate', showMoreResults:'Arată mai multe rezultate', noResultsFound:'Nu s-au găsit rezultate', tryDifferentKeywords:'Încercați alte cuvinte cheie sau verificați ortografia', didYouMean:'Poate ați vrut să spuneți:', quickAnswer:'Răspuns rapid', searchingMultipleSources:'Se caută în mai multe surse...', connectionError:'Eroare de conexiune. Verificați internetul și încercați din nou.', tagline:'Mai rapid decât Google. Aceleași rezultate. Fără API. Fără index.', resultsStats:'Aproximativ {count} rezultate — Fără API. Fără index. Mai rapid.', settingsTitle:'Setări', language:'Limbă', darkMode:'Mod întunecat', close:'Închide', wikipedia:'Wikipedia' },
        hu: { about:'Névjegy', images:'Képek', searchPlaceholder:'Keresés a Gatewayben vagy URL megadása', gatewaySearch:'Gateway-keresés', feelingLucky:'Szerencsém van', unitedKingdom:'Egyesült Királyság', advertising:'Hirdetés', business:'Vállalkozások', howSearchWorks:'Hogyan működik a keresés', privacy:'Adatvédelem', terms:'Feltételek', settings:'Beállítások', all:'Összes', showMoreResults:'Több találat mutatása', noResultsFound:'Nincs találat', tryDifferentKeywords:'Próbáljon más kulcsszavakat vagy ellenőrizze a helyesírást', didYouMean:'Esetleg erre gondolt:', quickAnswer:'Gyors válasz', searchingMultipleSources:'Keresés több forrásban...', connectionError:'Kapcsolódási hiba. Ellenőrizze az internetkapcsolatot és próbálja újra.', tagline:'Gyorsabb, mint a Google. Ugyanazok az eredmények. Nincs API. Nincs index.', resultsStats:'Kb. {count} találat — Nincs API. Nincs index. Gyorsabb.', settingsTitle:'Beállítások', language:'Nyelv', darkMode:'Sötét mód', close:'Bezár', wikipedia:'Wikipédia' },
        el: { about:'Σχετικά', images:'Εικόνες', searchPlaceholder:'Αναζήτηση στο Gateway ή πληκτρολογήστε URL', gatewaySearch:'Αναζήτηση Gateway', feelingLucky:'Τυχερός', unitedKingdom:'Ηνωμένο Βασίλειο', advertising:'Διαφήμιση', business:'Επιχειρήσεις', howSearchWorks:'Πώς λειτουργεί η Αναζήτηση', privacy:'Απόρρητο', terms:'Όροι', settings:'Ρυθμίσεις', all:'Όλα', showMoreResults:'Εμφάνιση περισσότερων αποτελεσμάτων', noResultsFound:'Δεν βρέθηκαν αποτελέσματα', tryDifferentKeywords:'Δοκιμάστε διαφορετικές λέξεις-κλειδιά ή ελέγξτε την ορθογραφία', didYouMean:'Μήπως εννοείτε:', quickAnswer:'Γρήγορη απάντηση', searchingMultipleSources:'Αναζήτηση σε πολλαπλές πηγές...', connectionError:'Σφάλμα σύνδεσης. Ελέγξτε το διαδίκτυο και δοκιμάστε ξανά.', tagline:'Ταχύτερο από το Google. Ίδια αποτελέσματα. Χωρίς API. Χωρίς ευρετήριο.', resultsStats:'Περίπου {count} αποτελέσματα — Χωρίς API. Χωρίς ευρετήριο. Ταχύτερο.', settingsTitle:'Ρυθμίσεις', language:'Γλώσσα', darkMode:'Σκοτεινή λειτουργία', close:'Κλείσιμο', wikipedia:'Βικιπαίδεια' }
    };

    const LANG_LIST = [
        { code:'en', name:'English' }, { code:'es', name:'Español' }, { code:'fr', name:'Français' },
        { code:'de', name:'Deutsch' }, { code:'it', name:'Italiano' }, { code:'pt', name:'Português' },
        { code:'ru', name:'Русский' }, { code:'ja', name:'日本語' }, { code:'zh-CN', name:'简体中文' },
        { code:'zh-TW', name:'繁體中文' }, { code:'ko', name:'한국어' }, { code:'ar', name:'العربية' },
        { code:'hi', name:'हिन्दी' }, { code:'bn', name:'বাংলা' }, { code:'tr', name:'Türkçe' },
        { code:'nl', name:'Nederlands' }, { code:'pl', name:'Polski' }, { code:'sv', name:'Svenska' },
        { code:'da', name:'Dansk' }, { code:'fi', name:'Suomi' }, { code:'no', name:'Norsk' },
        { code:'cs', name:'Čeština' }, { code:'ro', name:'Română' }, { code:'hu', name:'Magyar' },
        { code:'el', name:'Ελληνικά' }
    ];

    const TZ_COUNTRY = {
        'Europe/London':'GB','Europe/Paris':'FR','Europe/Berlin':'DE','Europe/Rome':'IT',
        'Europe/Madrid':'ES','Europe/Lisbon':'PT','Europe/Moscow':'RU','Europe/Athens':'GR',
        'Europe/Amsterdam':'NL','Europe/Brussels':'BE','Europe/Stockholm':'SE',
        'Europe/Copenhagen':'DK','Europe/Oslo':'NO','Europe/Helsinki':'FI','Europe/Prague':'CZ',
        'Europe/Bucharest':'RO','Europe/Budapest':'HU','Europe/Warsaw':'PL','Europe/Vienna':'AT',
        'Europe/Zurich':'CH','Europe/Istanbul':'TR','Asia/Tokyo':'JP','Asia/Seoul':'KR',
        'Asia/Shanghai':'CN','Asia/Taipei':'TW','Asia/Hong_Kong':'HK','Asia/Kolkata':'IN',
        'Asia/Dhaka':'BN','Asia/Bangkok':'TH','Asia/Singapore':'SG','Asia/Dubai':'AE',
        'Asia/Jerusalem':'IL','Asia/Riyadh':'SA','Asia/Tehran':'IR','Asia/Baghdad':'IQ',
        'America/New_York':'US','America/Chicago':'US','America/Denver':'US','America/Los_Angeles':'US',
        'America/Toronto':'CA','America/Vancouver':'CA','America/Mexico_City':'MX',
        'America/Sao_Paulo':'BR','America/Argentina/Buenos_Aires':'AR',
        'America/Bogota':'CO','America/Santiago':'CL','America/Lima':'PE',
        'Australia/Sydney':'AU','Australia/Melbourne':'AU','Pacific/Auckland':'NZ',
        'Africa/Cairo':'EG','Africa/Johannesburg':'ZA','Africa/Lagos':'NG','Africa/Nairobi':'KE'
    };

    function detectCountry() {
        try {
            const tz = Intl.DateTimeFormat().resolvedOptions().timeZone;
            return TZ_COUNTRY[tz] || 'GB';
        } catch(_) { return 'GB'; }
    }

    function countryName(code, lang) {
        try { return new Intl.DisplayNames([lang], { type: 'region' }).of(code); }
        catch(_) { return code; }
    }

    function detectBrowserLang() {
        try {
            const raw = (navigator.language || navigator.userLanguage || 'en').split('-')[0];
            return LANG[raw] ? raw : 'en';
        } catch(_) { return 'en'; }
    }

    let _lang = localStorage.getItem('gw-lang') || 'auto';
    if (_lang !== 'auto' && !LANG[_lang]) _lang = 'auto';
    if (_lang === 'auto') _lang = detectBrowserLang();

    function _t(key, vars) {
        let s = (LANG[_lang] && LANG[_lang][key]) || LANG.en[key] || key;
        if (vars) for (const k in vars) s = s.split('{'+k+'}').join(vars[k]);
        return s;
    }

    function updateCountry() {
        const code = detectCountry();
        const stored = localStorage.getItem('gw-lang') || 'auto';
        const displayLang = stored === 'auto' ? detectBrowserLang() : stored;
        const name = countryName(code, displayLang);
        document.querySelectorAll('[data-i18n-country]').forEach(el => el.textContent = name);
    }

    function _applyTranslations() {
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') return;
            el.textContent = _t(key);
        });
        document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
            el.placeholder = _t(el.getAttribute('data-i18n-placeholder'));
        });
        document.title = 'Gateway';
        const sel = $('langSelect');
        if (sel) sel.value = _lang;
        const dt = $('settingsDarkToggle');
        if (dt) dt.checked = document.body.classList.contains('dark');
    }

    window.setLanguage = function(lang) {
        if (lang !== 'auto' && !LANG[lang]) return;
        localStorage.setItem('gw-lang', lang);
        _lang = lang === 'auto' ? detectBrowserLang() : lang;
        window.gatewayLang = _lang;
        _applyTranslations();
        updateCountry();
        const sel = $('langSelect');
        if (sel) sel.value = lang;
    };
    window.gatewayLang = _lang;

    // ======================== SETTINGS ========================

    window.openSettings = function(e) {
        const overlay = $('settingsOverlay');
        if (overlay) overlay.classList.add('show');
        const dt = $('settingsDarkToggle');
        if (dt) dt.checked = document.body.classList.contains('dark');
        const sel = $('langSelect');
        if (sel) sel.value = _lang;
    };

    window.closeSettings = function(e) {
        if (e && e.target !== e.currentTarget) return;
        const overlay = $('settingsOverlay');
        if (overlay) overlay.classList.remove('show');
    };

    // ======================== STATE ========================

    let allResults = [];
    const PER_PAGE = 12;
    let activeTab = 'all';
    let spellSuggestion = null;
    let lastQuery = '';
    window._gwPage = 1;
    window._gwLoaded = 0;

    const SOURCE_TABS = { all: null, images: 'image' };

    // ======================== DARK MODE ========================

    function applyDark(enabled) {
        document.documentElement.classList.toggle('dark', enabled);
        document.body.classList.toggle('dark', enabled);
        localStorage.setItem('gw-dark', enabled ? '1' : '0');
        document.querySelectorAll('.sun').forEach(el => el.style.display = enabled ? 'none' : '');
        document.querySelectorAll('.moon').forEach(el => el.style.display = enabled ? '' : 'none');
        const dt = $('settingsDarkToggle');
        if (dt) dt.checked = enabled;
    }
    window.toggleDarkMode = function() {
        applyDark(!document.documentElement.classList.contains('dark'));
    };
    if (localStorage.getItem('gw-dark') === '1') applyDark(true);

    // ======================== CLEAR BUTTON ========================

    window.clearSearch = function(inputId) {
        const input = $(inputId);
        if (input) { input.value = ''; input.focus(); }
        updateClearBtn(inputId);
    };
    function updateClearBtn(inputId) {
        const input = $(inputId);
        const btn = inputId === 'mainSearchInput' ? $('homeClearBtn') : $('resultsClearBtn');
        if (input && btn) btn.classList.toggle('visible', input.value.length > 0);
    }

    // ======================== AUTOCOMPLETE ========================

    const autoState = {};

    function initAutocomplete(inputId, dropdownId) {
        const input = $(inputId);
        const dd = $(dropdownId);
        if (!input || !dd) return;
        const wrapper = input.closest('.search-bar-wrapper');
        let timer, sug = [], hl = -1, open = false;

        function close() {
            dd.classList.remove('show');
            if (wrapper) wrapper.classList.remove('dd-open');
            open = false; hl = -1;
        }
        function highlight(idx) {
            const items = dd.querySelectorAll('.autocomplete-item');
            items.forEach((el, i) => el.classList.toggle('highlighted', i === idx));
            hl = idx;
            if (idx >= 0 && items[idx]) items[idx].scrollIntoView({ block: 'nearest' });
        }
        function select(idx) {
            if (idx < 0 || idx >= sug.length) return;
            input.value = sug[idx]; close(); updateClearBtn(inputId); doSearch(inputId);
        }
        function render(list) {
            sug = list; dd.innerHTML = '';
            if (!list.length) { close(); return; }
            list.forEach((s, i) => {
                const div = document.createElement('div');
                div.className = 'autocomplete-item';
                const idx = s.toLowerCase().indexOf(input.value.toLowerCase());
                div.innerHTML = '<span class="autocomplete-icon"><svg width="16" height="16" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"/></svg></span>'
                    + (idx >= 0 ? s.slice(0, idx) + '<span class="match">' + s.slice(idx, idx + input.value.length) + '</span>' + s.slice(idx + input.value.length) : s);
                div.onclick = () => { input.value = s; close(); updateClearBtn(inputId); doSearch(inputId); };
                div.onmouseenter = () => highlight(i);
                dd.appendChild(div);
            });
            dd.classList.add('show');
            if (wrapper) wrapper.classList.add('dd-open');
            open = true; hl = -1;
        }

        input.addEventListener('input', function() {
            clearTimeout(timer);
            const val = this.value.trim();
            if (val.length < 2) { close(); return; }
            timer = setTimeout(async () => {
                if (typeof getSuggestions !== 'undefined') {
                    const sugs = await getSuggestions(val);
                    render(sugs);
                }
            }, 250);
        });
        input.addEventListener('keydown', function(e) {
            if (!open) return;
            if (e.key === 'ArrowDown') { e.preventDefault(); highlight(hl < sug.length - 1 ? hl + 1 : 0); }
            else if (e.key === 'ArrowUp') { e.preventDefault(); highlight(hl > 0 ? hl - 1 : sug.length - 1); }
            else if (e.key === 'Enter' && hl >= 0) { e.preventDefault(); select(hl); }
            else if (e.key === 'Escape') close();
        });
        input.addEventListener('blur', () => setTimeout(close, 150));
        input.addEventListener('focus', function() {
            if (sug.length && this.value.trim().length >= 2) {
                dd.classList.add('show');
                if (wrapper) wrapper.classList.add('dd-open');
                open = true;
            }
        });
        autoState[inputId] = { close };
    }

    function doSearch(inputId) {
        const input = $(inputId);
        if (!input || !input.value.trim()) return;
        if (autoState[inputId]) autoState[inputId].close();
        performSearch(inputId);
    }

    // ======================== XPDEV SECRET LIST ========================

    const XPDEV_SITES = [
        {
            title: 'XPDevs',
            url: 'https://xpdevs.github.io',
            description: 'The official XPDevs website — innovative software development and design by XPDevs.',
            fullSnippet: 'XPDevs creates innovative software solutions. Visit the official website for projects, tools, and more.',
            extract: 'The official XPDevs website — innovative software development and design.',
            thumbnail: null,
            source: 'special',
            sourceLabel: 'XPDevs',
            resultType: 'web',
            score: 99999,
            domain: 'xpdevs.github.io',
            suggestion: null
        },
        {
            title: 'XPDevs GitHub',
            url: 'https://github.com/XPDevs',
            description: 'Explore XPDevs open-source projects on GitHub — tools, libraries, and experiments.',
            fullSnippet: 'Explore XPDevs open-source projects on GitHub.',
            extract: 'Explore XPDevs open-source projects on GitHub.',
            thumbnail: null,
            source: 'special',
            sourceLabel: 'GitHub',
            resultType: 'web',
            score: 99998,
            domain: 'github.com',
            suggestion: null
        },
        {
            title: 'Gateway Search',
            url: 'https://xpdevs.github.io/gateway',
            description: 'Gateway — the multi-source search engine by XPDevs. Fast, private, no API keys needed.',
            fullSnippet: 'Gateway — the multi-source search engine by XPDevs.',
            extract: 'Gateway — the multi-source search engine by XPDevs.',
            thumbnail: null,
            source: 'special',
            sourceLabel: 'Gateway',
            resultType: 'web',
            score: 99997,
            domain: 'xpdevs.github.io',
            suggestion: null
        }
    ];

    function _boostXpdevs(results, query) {
        if (/^(xpdevs|gateway)$/i.test(query.trim())) {
            const existing = new Set(results.map(r => r.url));
            const toAdd = XPDEV_SITES.filter(s => !existing.has(s.url));
            results.unshift(...toAdd);
        }
        return results;
    }

    // ======================== SEARCH ========================

    async function performSearch(inputId) {
        const input = $(inputId);
        if (!input) return;
        const q = input.value.trim();
        if (!q) return;

        lastQuery = q;
        spellSuggestion = null;

        if ($('mainSearchInput')) $('mainSearchInput').value = q;
        if ($('resultsSearchInput')) $('resultsSearchInput').value = q;
        updateClearBtn('mainSearchInput');
        updateClearBtn('resultsSearchInput');

        if (autoState.mainSearchInput) autoState.mainSearchInput.close();
        if (autoState.resultsSearchInput) autoState.resultsSearchInput.close();

        if ($('homeUI')) $('homeUI').style.display = 'none';
        if ($('resultsUI')) $('resultsUI').classList.add('visible');

        if ($('answerArea')) $('answerArea').innerHTML = '';
        if ($('didYouMean')) $('didYouMean').innerHTML = '';
        if ($('emptyState')) $('emptyState').style.display = 'none';
        if ($('resultsList')) {
            $('resultsList').innerHTML = `
                <div class="loading-state">
                    <div class="spinner"></div>
                    <p>${_t('searchingMultipleSources')}</p>
                </div>`;
        }
        if ($('resultStats')) $('resultStats').textContent = '';
        if ($('loadMoreArea')) $('loadMoreArea').style.display = 'none';

        try {
            allResults = await window.gatewayCrawl(q);
            allResults = _boostXpdevs(allResults, q);

            if (allResults.length) {
                try { const s = await window.gatewaySpellCheck(q); spellSuggestion = s; } catch(_) {}
            }

            window._gwPage = 1;
            window._gwLoaded = 0;

            try {
                const url = new URL(window.location);
                url.searchParams.set('query', q);
                window.history.replaceState({ query: q }, '', url.toString());
            } catch(_) {}

            window._gwRender();
        } catch(_) {
            if ($('resultsList')) {
                $('resultsList').innerHTML = '<p style="color:#ff4d4d;text-align:center;padding:32px">' + _t('connectionError') + '</p>';
            }
        }
    }
    window.performSearch = performSearch;

    // ======================== RENDER ========================

    function resultBadge(r) {
        if (r.source === 'special') return '<span class="badge special">\u2605</span>';
        if (r.resultType === 'web') return '<span class="badge web">\u25C9</span>';
        return '<span class="badge wiki">W</span>';
    }

    function renderItem(r) {
        if (r.source === 'special') {
            const urlDisplay = (r.url || '').replace(/^https?:\/\//, '');
            return `
                <div class="result-item">
                    <div class="result-item-url">
                        <span class="badge special">\u2605</span>
                        <a href="${r.url}" target="_self">${urlDisplay}</a>
                    </div>
                    <a class="result-item-title" href="${r.url}" target="_self">${r.title}</a>
                    <div class="result-item-desc">${r.description || ''}</div>
                </div>`;
        }
        const isWeb = r.resultType === 'web';
        const urlDisplay = (r.url || '').replace(/^https?:\/\//, '').replace(/\/$/, '');
        const thumb = r.thumbnail && !isWeb
            ? `<img class="result-item-thumb" src="${r.thumbnail}" alt="" loading="lazy" onerror="this.style.display='none'">`
            : '';
        const badge = resultBadge(r);
        return `
            <div class="result-item">
                ${thumb}
                <div class="result-item-url">
                    ${badge}
                    <a href="${r.url}" target="_self">${urlDisplay}</a>
                </div>
                <a class="result-item-title" href="${r.url}" target="_self">${r.title}</a>
                <div class="result-item-desc">${r.description || ''}</div>
            </div>`;
    }

    function renderGridItem(r) {
        return `
            <div class="image-grid-item">
                <a href="${r.url}" target="_self">
                    <img src="${r.thumbnail}" alt="${r.title}" loading="lazy" onerror="this.closest('.image-grid-item').style.display='none'">
                    <div class="img-label">${r.title}</div>
                </a>
            </div>`;
    }

    window._gwRender = function() {
        const isImages = activeTab === 'images';
        const filtered = isImages ? allResults.filter(r => r.thumbnail) : allResults;

        if ($('resultStats')) {
            $('resultStats').textContent = _t('resultsStats', { count: filtered.length });
        }

        const dym = $('didYouMean');
        if (dym) {
            if (spellSuggestion && activeTab === 'all') {
                dym.innerHTML = `${_t('didYouMean')} <a onclick="searchSuggestion('${spellSuggestion.replace(/['"\\]/g, '')}')">${spellSuggestion}</a>`;
            } else dym.innerHTML = '';
        }

        const answerArea = $('answerArea');
        if (answerArea) {
            if (activeTab === 'all') {
                const wikiResult = filtered.find(r => (r.resultType === 'wiki' || r.source === 'special') && r.extract && r.extract.length > 40);
                if (wikiResult) {
                    const concise = wikiResult.extract.split('. ')[0] + '.';
                    const webLinks = filtered.filter(r =>
                        r.resultType === 'web' && r.url !== wikiResult.url && !r.url.includes('wikipedia.org') && !r.url.includes('duckduckgo.com')
                    ).slice(0, 4);
                    let linksHtml = '';
                    if (webLinks.length) {
                        linksHtml = webLinks.map(r =>
                            `<a href="${r.url}" target="_self" class="answer-link">${r.domain || r.title}</a>`
                        ).join('');
                    }
                    answerArea.innerHTML = `
                        <div class="answer-box">
                            <h2>${_t('quickAnswer')}</h2>
                            <p>${concise}</p>
                            <div class="answer-meta">
                                <span class="badge">${_t('wikipedia')}</span>
                                <a href="${wikiResult.url}" target="_self">${wikiResult.title}</a>
                                ${linksHtml ? '<span class="answer-sep">·</span>' + linksHtml : ''}
                            </div>
                        </div>`;
                } else answerArea.innerHTML = '';
            } else answerArea.innerHTML = '';
        }

        const list = $('resultsList');
        const empty = $('emptyState');
        const loadMore = $('loadMoreArea');
        if (!list) return;

        if (!filtered.length) {
            list.innerHTML = '';
            if (empty) empty.style.display = '';
            if (loadMore) loadMore.style.display = 'none';
            return;
        }
        if (empty) empty.style.display = 'none';

        window._gwLoaded = window._gwLoaded || 0;
        const start = window._gwLoaded;
        const end = Math.min(start + PER_PAGE, filtered.length);
        const newItems = filtered.slice(start, end);

        if (start === 0) {
            if (isImages) {
                list.innerHTML = '<div class="image-grid">' + newItems.map(renderGridItem).join('') + '</div>';
            } else {
                list.innerHTML = newItems.map(renderItem).join('');
            }
            window.scrollTo(0, 0);
        } else {
            if (isImages) {
                const grid = list.querySelector('.image-grid');
                if (grid) grid.insertAdjacentHTML('beforeend', newItems.map(renderGridItem).join(''));
            } else {
                list.insertAdjacentHTML('beforeend', newItems.map(renderItem).join(''));
            }
        }

        window._gwLoaded = end;
        if (loadMore) {
            loadMore.style.display = (window._gwLoaded < filtered.length) ? 'flex' : 'none';
        }
    };

    window.loadMoreResults = function() { window._gwRender(); };

    // ======================== TABS ========================

    window.switchTab = function(tab) {
        activeTab = tab;
        document.querySelectorAll('.tab-item').forEach(el => el.classList.toggle('active', el.dataset.tab === tab));
        window._gwPage = 1;
        window._gwLoaded = 0;
        if ($('resultsList')) $('resultsList').innerHTML = '';
        window._gwRender();
    };

    // ======================== LUCKY SEARCH ========================

    window.luckySearch = function() {
        const val = $('mainSearchInput').value.trim();
        if (!val) return;
        performSearch('mainSearchInput').then(() => {
            const filtered = activeTab === 'images' ? allResults.filter(r => r.thumbnail) : allResults;
            if (filtered.length) window.open(filtered[0].url, '_self');
        });
    };

    window.searchSuggestion = function(term) {
        if ($('mainSearchInput')) $('mainSearchInput').value = term;
        if ($('resultsSearchInput')) $('resultsSearchInput').value = term;
        updateClearBtn('mainSearchInput');
        updateClearBtn('resultsSearchInput');
        performSearch('resultsSearchInput');
    };

    // ======================== KEY HANDLING ========================

    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') {
            if ($('settingsOverlay') && $('settingsOverlay').classList.contains('show')) {
                closeSettings(); return;
            }
            if ($('resultsUI') && $('resultsUI').classList.contains('visible')) {
                if ($('resultsSearchInput')) $('resultsSearchInput').focus();
            } else {
                if ($('mainSearchInput')) $('mainSearchInput').focus();
            }
        }
        if (e.key === '/' && !['INPUT', 'TEXTAREA'].includes(e.target.tagName)) {
            e.preventDefault();
            const input = ($('resultsUI') && $('resultsUI').classList.contains('visible')) ? $('resultsSearchInput') : $('mainSearchInput');
            if (input) input.focus();
        }
    });

    // ======================== INIT ========================

    if ($('mainSearchInput')) {
        $('mainSearchInput').addEventListener('keydown', e => { if (e.key === 'Enter') doSearch('mainSearchInput'); });
        $('mainSearchInput').addEventListener('input', () => updateClearBtn('mainSearchInput'));
    }
    if ($('resultsSearchInput')) {
        $('resultsSearchInput').addEventListener('keydown', e => { if (e.key === 'Enter') doSearch('resultsSearchInput'); });
        $('resultsSearchInput').addEventListener('input', () => updateClearBtn('resultsSearchInput'));
    }

    initAutocomplete('mainSearchInput', 'homeAutocomplete');
    initAutocomplete('resultsSearchInput', 'resultsAutocomplete');

    function getQueryParam() {
        const params = new URLSearchParams(window.location.search);
        return params.get('query') || params.get('q') || '';
    }

    window.addEventListener('popstate', function() {
        const q = getQueryParam();
        if ($('resultsUI') && $('resultsUI').classList.contains('visible')) {
            if (q) {
                if ($('mainSearchInput')) $('mainSearchInput').value = q;
                if ($('resultsSearchInput')) $('resultsSearchInput').value = q;
                performSearch('resultsSearchInput');
            } else {
                location.reload();
            }
        }
    });

    try {
        const q = getQueryParam();
        if (q) {
            if ($('mainSearchInput')) $('mainSearchInput').value = q;
            if ($('resultsSearchInput')) $('resultsSearchInput').value = q;
            performSearch('mainSearchInput');
        }
    } catch(_) {}

    _applyTranslations();
    updateCountry();
    window.setLanguage(_lang);
})();
