/**
 * Gateway Gatherer Script
 * Handles the discovery and organization of information.
 */

/**
 * The main function to find information.
 * It now performs a live gather from a source bridge.
 * @param {string} term - The word or phrase to look for.
 * @returns {Promise<Array>} - A list of matching entries.
 */
async function gatewayCrawl(term) {
    if (!term) return [];

    // The bridge for gathering live data
    const bridge = `https://en.wikipedia.org/w/api.php?action=query&generator=search&gsrsearch=${encodeURIComponent(term)}&gsrlimit=10&prop=extracts|info&exintro&explaintext&exsentences=2&inprop=url&format=json&origin=*`;

    try {
        const response = await fetch(bridge);
        const data = await response.json();

        if (!data.query || !data.query.pages) {
            return [];
        }

        // Organizing the gathered information into our Gateway structure
        const pages = data.query.pages;
        const findings = Object.values(pages).map(page => ({
            title: page.title,
            url: page.fullurl,
            description: page.extract || "No summary available for this entry.",
            // Creating keywords for the librarian logic
            keywords: page.title.toLowerCase().split(' ')
        }));

        return findings;

    } catch (err) {
        // Fallback if the connection is interrupted
        return [];
    }
}
